//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

cbuffer DecalGenVSConstants: register( b0 )
{
	float4x4 World : packoffset( c0 );
}

cbuffer DecalGenGSConstants: register( b0 )
{
	float4 ArrClipPlanes[6] : packoffset( c0 );
	float2 DecalSize        : packoffset( c6 );
	float3 HitNorm	        : packoffset( c7 );
}

#define MAX_NEW_VERT 6

//--------------------------------------------------------------------------------------
// Vertex shader input structure
//--------------------------------------------------------------------------------------

struct VERT_INPUT_OUTPUT
{
    float3 Pos : POSITION;
    float3 Norm : NORMAL;
    float2 Tex : TEXCOORD0;
};

VERT_INPUT_OUTPUT DecalGenVS(VERT_INPUT_OUTPUT input)
{
	VERT_INPUT_OUTPUT output;
	
	output.Pos = mul(float4(input.Pos, 1.0), World).xyz;
	output.Norm = mul(input.Norm, (float3x3)World);
	output.Tex = input.Tex;
	
	return output;
}

void PlaneSegIntersec(float4 p1, float3 norm1, float4 p2, float3 norm2, float4 plane, out float4 intersectPos, out float3 intersectNorm)
{
	float3 segDir = p2.xyz - p1.xyz;
	float segDist = length(segDir);
	segDir = segDir / segDist;
	float fDenom = dot(plane.xyz, segDir);
	
	float fDist = -dot(plane, p1) / fDenom;
	intersectPos = float4(p1.xyz + fDist * segDir, 1.0);
	
	// Calculate the normal
	intersectNorm = lerp(norm1, norm2, fDist / segDist);
	intersectNorm = normalize(intersectNorm);
}

void PolyPlane(float4 arrVerts[MAX_NEW_VERT], float3 arrNormals[MAX_NEW_VERT], float arrDot[MAX_NEW_VERT], uint iNumVerts, float4 plane, out float4 arrNewVerts[MAX_NEW_VERT], out float3 arrNewNormals[MAX_NEW_VERT], out uint iCurNewVert)
{
	arrNewVerts = (float4[MAX_NEW_VERT])100000.0f;
	iCurNewVert = 0;
	
	for(uint i=0; i < iNumVerts; i++)
	{
		if(arrDot[i] >= 0)
		{
			arrNewVerts[iCurNewVert] = arrVerts[i];
			arrNewNormals[iCurNewVert] = arrNormals[i];
			iCurNewVert++;
			
			if(arrDot[(i+1)%iNumVerts] < 0)
			{
				PlaneSegIntersec(arrVerts[i], arrNormals[i], arrVerts[(i+1)%iNumVerts], arrNormals[(i+1)%iNumVerts], plane, arrNewVerts[iCurNewVert], arrNewNormals[iCurNewVert]);
				iCurNewVert++;
			}
		}
		else if(arrDot[(i+1)%iNumVerts] >= 0)
		{
			PlaneSegIntersec(arrVerts[i], arrNormals[i], arrVerts[(i+1)%iNumVerts], arrNormals[(i+1)%iNumVerts], plane, arrNewVerts[iCurNewVert], arrNewNormals[iCurNewVert]);
			iCurNewVert++;
		}
	}
}

[maxvertexcount(12)] // Max 4 triangles x 3 vertices
void DecalGenGS(triangle VERT_INPUT_OUTPUT input[3], inout TriangleStream<VERT_INPUT_OUTPUT> TriStream)
{
	uint nNumVerts = 0;
	float4 arrNewVerts[MAX_NEW_VERT] = (float4[MAX_NEW_VERT])100000.0;
	float3 arrNewNormals[MAX_NEW_VERT] = (float3[MAX_NEW_VERT])0.0;
	uint iIn[MAX_NEW_VERT] = (uint[MAX_NEW_VERT])0;
	float arrDot[MAX_NEW_VERT] = (float[MAX_NEW_VERT])0;
	
	arrNewVerts[0] = float4(input[0].Pos, 1.0);
	arrNewVerts[1] = float4(input[1].Pos, 1.0);
	arrNewVerts[2] = float4(input[2].Pos, 1.0);
	arrNewNormals[0] = input[0].Norm;
	arrNewNormals[1] = input[1].Norm;
	arrNewNormals[2] = input[2].Norm;
	
	// Make sure the triangle is not facing away from the hit ray
	float3 AB = arrNewVerts[1].xyz - arrNewVerts[0].xyz;
	float3 AC = arrNewVerts[2].xyz - arrNewVerts[0].xyz;
	float3 faceNorm = cross(AB, AC);
	float fDot = dot(faceNorm, HitNorm);
	nNumVerts = 3 * (fDot > 0.01);
	
	// Clip the triangle with each one of the planes
	for(uint iCurPlane=0; iCurPlane < 6; iCurPlane++)
	{
		// First check the cull status for each vertex
		for(uint i=0; i < MAX_NEW_VERT; i++ )
		{
			arrDot[i] = dot(ArrClipPlanes[iCurPlane], arrNewVerts[i]);
		}
		
		// Calculate the new vertices based on the culling status
		uint nNewNumVerts = 0;
		PolyPlane(arrNewVerts, arrNewNormals, arrDot, nNumVerts, ArrClipPlanes[iCurPlane], arrNewVerts, arrNewNormals, nNewNumVerts);
		nNumVerts = nNewNumVerts;
	}
	
	VERT_INPUT_OUTPUT output = (VERT_INPUT_OUTPUT)0;
	
	// Add the new triangles to the stream
	for(uint nCurVert = 1; nCurVert < nNumVerts-1 && nNumVerts > 0; nCurVert++)
	{
		output.Pos = arrNewVerts[0].xyz;
		output.Norm = arrNewNormals[0];
		output.Tex.x = dot(arrNewVerts[0], ArrClipPlanes[1]);
		output.Tex.y = dot(arrNewVerts[0], ArrClipPlanes[3]);
		output.Tex = output.Tex / DecalSize;
		TriStream.Append( output );
		output.Pos = arrNewVerts[nCurVert].xyz;
		output.Norm = arrNewNormals[nCurVert];
		output.Tex.x = dot(arrNewVerts[nCurVert], ArrClipPlanes[1]);
		output.Tex.y = dot(arrNewVerts[nCurVert], ArrClipPlanes[3]);
		output.Tex = output.Tex / DecalSize;
		TriStream.Append( output );
		output.Pos = arrNewVerts[nCurVert+1].xyz;
		output.Norm = arrNewNormals[nCurVert+1];
		output.Tex.x = dot(arrNewVerts[nCurVert+1], ArrClipPlanes[1]);
		output.Tex.y = dot(arrNewVerts[nCurVert+1], ArrClipPlanes[3]);
		output.Tex = output.Tex / DecalSize;
		TriStream.Append( output );
		TriStream.RestartStrip();
	}
}