#pragma once

namespace common
{

// The derived class passes its gp* global and its this pointer. The global is set before the derived class's members
// are constructed, unless the derived class passes nullptr in place of this and calls Register(this) once the state
// that readers of the global need exists. It is cleared after those members are destroyed.
template <typename T>
class Singleton
{
public:

	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

protected:

	Singleton(T*& rpSlot, T* pSelf)
	: mrpSlot(rpSlot)
	{
		ASSERT(mrpSlot == nullptr);
		Register(pSelf);
	}

	~Singleton()
	{
		mrpSlot = nullptr;
	}

	void Register(T* pSelf)
	{
		mrpSlot = pSelf;
	}

private:

	T*& mrpSlot;
};

} // namespace common
