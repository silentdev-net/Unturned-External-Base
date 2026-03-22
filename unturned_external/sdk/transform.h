#pragma once
#include <cstdint>
#include <corecrt_math.h>
#include <string>






struct vector3
{
	float x, y, z;

	inline vector3 operator*(float rhs) const { return { x * rhs, y * rhs, z * rhs }; }
	inline vector3 operator+(const vector3& rhs) const { return { x + rhs.x, y + rhs.y, z + rhs.z }; }
	inline vector3 operator-(const vector3& rhs) const { return { x - rhs.x, y - rhs.y, z - rhs.z }; }
	inline vector3 operator*(const vector3& rhs) const { return { x * rhs.x, y * rhs.y, z * rhs.z }; }
	inline vector3 operator/(const vector3& rhs) const { return { x / rhs.x, y / rhs.y, z / rhs.z }; }
	inline vector3 operator-() const { return { -x, -y, -z }; }

	
	inline vector3 normalized() const
	{
		float len = sqrtf(x * x + y * y + z * z);
		return { x / len, y / len, z / len };
	}

	inline float distance(const vector3& other) const {
		return sqrtf(powf(x - other.x, 2) + powf(y - other.y, 2) + powf(z - other.z, 2));
	}

	std::string toString()
	{
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
	}
};

struct vector4
{
	float x, y, z, w;
	inline vector4 operator*(float rhs) const { return { x * rhs, y * rhs, z * rhs, w * rhs }; }
	inline vector4 operator+(const vector4& rhs) const { return { x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w }; }
	inline vector4 operator-(const vector4& rhs) const { return { x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w }; }
	inline vector4 operator/(const vector4& rhs) const { return { x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w }; }
	inline vector4 operator-() const { return { -x, -y, -z, -w }; }

	inline vector4 operator*(const vector4& rhs) const
	{
		return {
			w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
			w * rhs.y + y * rhs.w + z * rhs.x - x * rhs.z,
			w * rhs.z + z * rhs.w + x * rhs.y - y * rhs.x,
			w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
		};
	}

	inline vector3 operator*(const vector3& rhs) const
	{
		float x = this->x * 2.0F;
		float y = this->y * 2.0F;
		float z = this->z * 2.0F;
		float xx = this->x * x;
		float yy = this->y * y;
		float zz = this->z * z;
		float xy = this->x * y;
		float xz = this->x * z;
		float yz = this->y * z;
		float wx = this->w * x;
		float wy = this->w * y;
		float wz = this->w * z;

		vector3 res;
		res.x = (1.0f - (yy + zz)) * rhs.x + (xy - wz) * rhs.y + (xz + wy) * rhs.z;
		res.y = (xy + wz) * rhs.x + (1.0f - (xx + zz)) * rhs.y + (yz - wx) * rhs.z;
		res.z = (xz - wy) * rhs.x + (yz + wx) * rhs.y + (1.0f - (xx + yy)) * rhs.z;

		return res;
	}

	inline vector4 conjugate() const { return { -x, -y, -z, w }; }
};

class TransformInternal
{
#pragma region Internal Structs
	
	struct TransformAccess
	{
		uintptr_t hierarchyAddr;
		int index;
	};

	class trsX
	{
	public:
		vector3 t;
	private:
		char pad_000C[4];
	public:
		vector4 q;
		vector3 s;
	private:
		char pad_0014[4];
	};
#pragma endregion
#pragma region List
	template <typename T>
	class ResizableBuffer
	{
	public:
		T* buffer;
		int allocated_capacity;
		int valid_count;

		inline ResizableBuffer() : buffer(nullptr), allocated_capacity(100), valid_count(0)
		{
			buffer = new T[allocated_capacity];
		}

		inline void updateBuffer(uintptr_t address, int needed_count)
		{
			
			if (needed_count < 0 || needed_count > 15000)
			{
				valid_count = 0;
				return;
			}
			

			if (needed_count > allocated_capacity)
			{
				if (buffer)
					delete[] buffer;

				allocated_capacity = needed_count * 2;
				buffer = new T[allocated_capacity];
			}

			read(address, (void*)buffer, sizeof(T) * needed_count);
			valid_count = needed_count;
		}

		inline T& operator[](int index)
		{
			if (index >= valid_count || index < 0)
			{
				printf("index out of bounds: %d >= %d. forgot to call updateBuffer?\n", index, valid_count);
				return buffer[0];
			}
			return buffer[index];
		}
	};
#pragma endregion
	
	static inline ResizableBuffer<trsX> trsBuffer;
	static inline ResizableBuffer<int> parentIndicesBuffer;

	uintptr_t address;

	TransformAccess transformAccess;
	uintptr_t localTransforms;
	uintptr_t parentIndices;

	
	inline void updateTrsXBuffer(int needed_count)
	{
		trsBuffer.updateBuffer(localTransforms, needed_count);
	}

	inline void updateParentIndicesBuffer(int needed_count)
	{
		parentIndicesBuffer.updateBuffer(parentIndices, needed_count);
	}

public:
	inline TransformInternal() : address(0), localTransforms(0), parentIndices(0) {
		transformAccess.hierarchyAddr = 0;
		transformAccess.index = 0;
	}

	inline TransformInternal(uintptr_t address)
	{
		this->address = address;
		this->transformAccess = read<TransformAccess>(address + 0x38);
		if (this->transformAccess.hierarchyAddr) {
			this->localTransforms = read<uintptr_t>(transformAccess.hierarchyAddr + 0x18);
			this->parentIndices = read<uintptr_t>(transformAccess.hierarchyAddr + 0x20);
		}
		else {
			this->localTransforms = 0;
			this->parentIndices = 0;
		}
	}

	
	
	

	inline vector3 localPosition()
	{
		if (!localTransforms) return { 0, 0, 0 };
		return read<trsX>(localTransforms + transformAccess.index * sizeof(trsX)).t;
	}

	inline vector3 localScale()
	{
		if (!localTransforms) return { 0, 0, 0 };
		return read<trsX>(localTransforms + transformAccess.index * sizeof(trsX)).s;
	}

	inline vector4 localRotation()
	{
		if (!localTransforms) return { 0, 0, 0, 1 };
		return read<trsX>(localTransforms + transformAccess.index * sizeof(trsX)).q;
	}

	inline vector3 position()
	{
		updateTrsXBuffer(transformAccess.index + 1);
		updateParentIndicesBuffer(transformAccess.index + 1);

		
		if (trsBuffer.valid_count == 0) return { 0, 0, 0 };

		vector3 worldPos = trsBuffer[transformAccess.index].t;
		int index = parentIndicesBuffer[transformAccess.index];
		int depth = 0;
		while (index >= 0 && index < 50000 && depth++ < 1000)
		{
			if (index >= trsBuffer.valid_count) updateTrsXBuffer(index + 1);
			if (index >= parentIndicesBuffer.valid_count) updateParentIndicesBuffer(index + 1);

			auto parent = trsBuffer[index];

			worldPos = parent.q * worldPos;
			worldPos = worldPos * parent.s;
			worldPos = worldPos + parent.t;

			index = parentIndicesBuffer[index];
		}

		return worldPos;
	}

	inline vector4 rotation()
	{
		updateTrsXBuffer(transformAccess.index + 1);
		updateParentIndicesBuffer(transformAccess.index + 1);

		
		if (trsBuffer.valid_count == 0) return { 0, 0, 0, 1 };

		vector4 worldRot = trsBuffer[transformAccess.index].q;
		int index = parentIndicesBuffer[transformAccess.index];
		int depth = 0;
		while (index >= 0 && index < 50000 && depth++ < 1000)
		{
			if (index >= trsBuffer.valid_count) updateTrsXBuffer(index + 1);
			if (index >= parentIndicesBuffer.valid_count) updateParentIndicesBuffer(index + 1);

			auto parent = trsBuffer[index];

			worldRot = parent.q * worldRot;

			index = parentIndicesBuffer[index];
		}

		return worldRot;
	}


	inline vector3 right()
	{
		static vector3 right = { 1, 0, 0 };
		return rotation() * right;
	}

	inline vector3 up()
	{
		static vector3 up = { 0, 1, 0 };
		return rotation() * up;
	}

	inline vector3 forward()
	{
		static vector3 forward = { 0, 0, 1 };
		return rotation() * forward;
	}

	
	inline vector3 TransformDirection(vector3 localDirection)
	{
		return rotation() * localDirection;
	}

	
	inline vector3 InverseTransformDirection(vector3 worldDirection)
	{
		return rotation().conjugate() * worldDirection;
	}

	
	inline vector3 TransformPoint(vector3 localPoint)
	{
		updateTrsXBuffer(transformAccess.index + 1);
		updateParentIndicesBuffer(transformAccess.index + 1);

		
		if (trsBuffer.valid_count == 0) return { 0, 0, 0 };

		vector3 worldPos = localPoint;
		int index = transformAccess.index;
		int depth = 0;
		while (index >= 0 && index < 50000 && depth++ < 1000)
		{
			if (index >= trsBuffer.valid_count) updateTrsXBuffer(index + 1);
			if (index >= parentIndicesBuffer.valid_count) updateParentIndicesBuffer(index + 1);

			auto parent = trsBuffer[index];

			worldPos = worldPos * parent.s;
			worldPos = parent.q * worldPos;
			worldPos = worldPos + parent.t;

			index = parentIndicesBuffer[index];
		}

		return worldPos;
	}

	
	inline vector3 InverseTransformPoint(vector3 worldPoint)
	{
		updateTrsXBuffer(transformAccess.index + 1);
		updateParentIndicesBuffer(transformAccess.index + 1);

		
		if (trsBuffer.valid_count == 0) return { 0, 0, 0 };

		vector3 worldPos = trsBuffer[transformAccess.index].t;
		vector4 worldRot = trsBuffer[transformAccess.index].q;

		vector3 localScale = trsBuffer[transformAccess.index].s;

		int index = parentIndicesBuffer[transformAccess.index];
		int depth = 0;
		while (index >= 0 && index < 50000 && depth++ < 1000)
		{
			if (index >= trsBuffer.valid_count) updateTrsXBuffer(index + 1);
			if (index >= parentIndicesBuffer.valid_count) updateParentIndicesBuffer(index + 1);

			auto parent = trsBuffer[index];

			worldPos = parent.q * worldPos;
			worldPos = worldPos * parent.s;
			worldPos = worldPos + parent.t;

			worldRot = parent.q * worldRot;

			index = parentIndicesBuffer[index];
		}

		vector3 local = worldRot.conjugate() * (worldPoint - worldPos);
		return local / localScale;
	}

	inline uintptr_t parent() {
		if (!address) return 0;
		return read<uintptr_t>(address + 0x90);
	}

	inline uintptr_t child(int index = 0) {
		if (!address) return 0;
		return read<uintptr_t>(read<uintptr_t>(address + 0x70) + (index * 0x8));
	}

	inline std::string name() {
		if (!address) return "";
		uintptr_t nameAddr = read<uintptr_t>(read<uintptr_t>(address + 0x30) + 0x60);
		if (!nameAddr) return "";
		char buffer[128] = { 0 };
		read(nameAddr, buffer, 128);
		return std::string(buffer);
	}

	inline uintptr_t child(const char* symbol) {
		if (!address) return 0;
		for (int i = 0; i < 200; i++) {
			uintptr_t t = child(i);
			if (!t) break;
			TransformInternal childTransform(t);
			std::string n = childTransform.name();
			if (n.empty()) break;
			if (n == symbol) return t;
		}
		return 0;
	}
};
