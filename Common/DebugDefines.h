#pragma once

#if defined(BT_DEBUG)
	#undef DEBUG
	#define DEBUG 1
	#undef NDEBUG
	#undef _DEBUG
	#define _DEBUG 1
	#undef _NDEBUG
#else
	#undef NDEBUG
	#define NDEBUG 1
	#undef DEBUG
	#undef _NDEBUG
	#define _NDEBUG 1
	#undef _DEBUG
#endif
