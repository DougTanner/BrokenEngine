#pragma once

namespace common
{

// The derived class passes its gp* global, which is cleared after the derived class's members are destroyed. It is
// set before those members are constructed, unless the derived class also passes nullptr and calls Register() once
// the state that readers of the global need exists.
template <typename T>
class Singleton
{
public:

	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;

protected:

	explicit Singleton(T*& rpSlot)
	: Singleton(rpSlot, nullptr)
	{
		Register();
	}

	Singleton(T*& rpSlot, [[maybe_unused]] std::nullptr_t pNull)
	: mrpSlot(rpSlot)
	{
		ASSERT(mrpSlot == nullptr);
	}

	~Singleton()
	{
		mrpSlot = nullptr;
	}

	void Register()
	{
		mrpSlot = static_cast<T*>(this);
	}

private:

	T*& mrpSlot;
};

} // namespace common
