#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <time.h>
#include "resource.h"
using namespace std;

struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
	XMFLOAT3 Normal;//necessary for Ians work
};

class ConstantBuffer
{
public:
	ConstantBuffer()
	{
		info = XMFLOAT4(1, 1, 1, 1);
		SunPos = XMFLOAT4(0, 0, 1000, 1);
		DayTimer = XMFLOAT4(0, 0, 0, 0);
	}
	XMMATRIX World;
	XMMATRIX View;
	XMMATRIX Projection;
	XMFLOAT4 info;
	XMFLOAT4 CameraPos;
	XMFLOAT4 SunPos;
	XMFLOAT4 DayTimer;
};

//********************************************
class StopWatchMicro_
{
private:
	LARGE_INTEGER last, frequency;
public:
	StopWatchMicro_()
	{
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&last);
	}

	long double elapse_micro()
	{
		LARGE_INTEGER now, dif;
		QueryPerformanceCounter(&now);
		dif.QuadPart = now.QuadPart - last.QuadPart;
		long double fdiff = (long double)dif.QuadPart;
		fdiff /= (long double)frequency.QuadPart;
		return fdiff*1000000.;
	}

	long double elapse_milli()
	{
		elapse_micro() / 1000.;
	}

	void start()
	{
		QueryPerformanceCounter(&last);
	}
};

//********************************************
class billboard
{
public:
	XMFLOAT3 position;
	float scale;
	float rotation;
	float transparency;
	
	billboard()
	{
		position = XMFLOAT3(0, 0, 0);
		scale = 1;
		transparency = 1;
	}

	XMMATRIX get_matrix(XMMATRIX &ViewMatrix)
	{
		XMMATRIX view, R, Ry, T, S;
		Ry = XMMatrixRotationX(-rotation);
		view = ViewMatrix;
		//eliminate camera translation:
		view._41 = view._42 = view._43 = 0.0;
		XMVECTOR det;
		R = XMMatrixInverse(&det, view); //inverse rotation
		T = XMMatrixTranslation(position.x, position.y, position.z);
		S = XMMatrixScaling(scale, scale, scale);
		return S*R*T*Ry;
	}

	XMMATRIX get_matrix_y(XMMATRIX &ViewMatrix) { }
};

//********************************************
class camera
{
public:
	int w, s, a, d, q, e;
	XMFLOAT3 position;
	XMFLOAT3 rotation;

	camera()
	{
		w = s = a = d = q = e = 0;
		position = rotation = XMFLOAT3(0, 0, 0);
	}

	void animation(float elapsed_microseconds)
	{
		XMMATRIX Rx, Ry, T;
		Ry = XMMatrixRotationY(-rotation.y);
		Rx = XMMatrixRotationX(-rotation.x);

		XMFLOAT3 forward = XMFLOAT3(0, 0, 1);
		XMVECTOR f = XMLoadFloat3(&forward);
		f = XMVector3TransformCoord(f, Rx*Ry);
		XMStoreFloat3(&forward, f);
		XMFLOAT3 side = XMFLOAT3(1, 0, 0);
		XMVECTOR si = XMLoadFloat3(&side);
		si = XMVector3TransformCoord(si, Rx*Ry);
		XMStoreFloat3(&side, si);
		XMFLOAT3 up = XMFLOAT3(0, 1, 0);
		XMVECTOR u = XMLoadFloat3(&up);
		u = XMVector3TransformCoord(u, Rx*Ry);
		XMStoreFloat3(&up, u);

		float speed = elapsed_microseconds / 100000.0;

		if (w)
		{
			position.x -= forward.x * speed;
			position.y -= forward.y * speed;
			position.z -= forward.z * speed;
		}
		if (s)
		{
			position.x += forward.x * speed;
			position.y += forward.y * speed;
			position.z += forward.z * speed;
		}
		if (d)
		{
			position.x -= side.x * speed;
			position.y -= side.y * speed;
			position.z -= side.z * speed;
		}
		if (a)
		{
			position.x += side.x * speed;
			position.y += side.y * speed;
			position.z += side.z * speed;
		}
		
		if (e)
		{
			position.x -= up.x * speed;
			position.y -= up.y * speed;
			position.z -= up.z * speed;
		}
		if (q)
		{
			position.x += up.x * speed;
			position.y += up.y * speed;
			position.z += up.z * speed;
		}
	}

	XMMATRIX get_matrix(XMMATRIX *view)
	{
		XMFLOAT2 unit_look;
		XMFLOAT3 unit_normal;
		XMMATRIX RY, RXZ, T;

		int x, y, z;

		//if (rotation.x >= (XM_PI/2) - 0.5) rotation.x = ((XM_PI/2) - 0.5);
		//if (rotation.x <= (-XM_PI/2) + 0.5) rotation.x = ((-XM_PI/2) + 0.5);
		if (rotation.x >= (XM_PI / 2)) rotation.x = ((XM_PI / 2));
		if (rotation.x <= (-XM_PI / 2)) rotation.x = ((-XM_PI / 2));

		unit_look = { cos(rotation.y), sin(-rotation.y) }; //make vector facing z (z = 1, x = 0)
		XMVECTOR ul = XMLoadFloat2(&unit_look);
		ul = XMVector2Orthogonal(XMVector2Normalize(ul)); // get axis of rotation (z = 0, x = 1)
		XMStoreFloat2(&unit_look, ul);

		unit_normal.x = unit_look.y;
		unit_normal.y = 0;
		unit_normal.z = unit_look.x;
		XMVECTOR ra = XMLoadFloat3(&unit_normal);

		RY = XMMatrixRotationY(rotation.y);
		RXZ = XMMatrixRotationAxis(ra, rotation.x);
		T = XMMatrixTranslation(position.x, position.y, position.z);
		return T*(*view) * RXZ * RY;
	}
};

//********************************************
class voxel_
{
public:
	SimpleVertex *v;
	ID3D11Buffer *vbuffer;

	int anz;
	voxel_()
	{
		vbuffer = NULL;
		anz = 256 * 256 * 256;
		v = new SimpleVertex[anz];
		for (int xx = 0; xx < 256; xx++)
			for (int yy = 0; yy < 256; yy++)
				for (int zz = 0; zz < 256; zz++)
				{
					v[xx + yy * 256 + zz * 256 * 256].Pos.x = xx;
					v[xx + yy * 256 + zz * 256 * 256].Pos.y = yy;
					v[xx + yy * 256 + zz * 256 * 256].Pos.z = zz;
				}
	}

	~voxel_()
	{
		delete[] v;
	}
};

//********************************************
class octree 
{
public:
	XMFLOAT3 min;
	XMFLOAT3 max;
	octree* parent;
	octree* children[8];
	vector<billboard> contained;
	int minsize;
	byte activenodes;

	octree() 
	{
		min = XMFLOAT3(0, 0, 0);
		max = XMFLOAT3(0, 0, 0);
		activenodes = 0;
		minsize = 1;
		parent = nullptr;
	}

	octree(XMFLOAT3 initMin, XMFLOAT3 initMax)
	{
		min = initMin;
		max = initMax;
		activenodes = 0;
		minsize = 1;
		parent = nullptr;
	}

	octree(XMFLOAT3 initMin, XMFLOAT3 initMax, octree *initParent) 
	{
		min = initMin;
		max = initMax;
		activenodes = 0;
		minsize = 1;
		parent = initParent;
	}

	~octree() 
	{
		delete[] children;
	}
};