#pragma warning(push, 0)
#pragma warning(disable : 4146 4702 4706 6001 6011 6067 6262 6268 6308 6330 6387 26051 26408 26409 26429 26432 26433 26434 26435 26438 26440 26443 26444 26447 26448 26451 26455 26456 26459 26460 26461 26466 26472 26475 26477 26481 26482 26485 26488 26498 26490 26493 26494 26495 26496 26497 26812 26814 26818 26819 28182 28183)

#define OPENEXR_EXPORT

#include "openexr/src/lib/OpenEXRCore/attributes.c"
#include "openexr/src/lib/OpenEXRCore/base.c"
#include "openexr/src/lib/OpenEXRCore/bytes.c"
#include "openexr/src/lib/OpenEXRCore/channel_list.c"
#include "openexr/src/lib/OpenEXRCore/coding.c"
#include "openexr/src/lib/OpenEXRCore/color_metadata.c"
#include "openexr/src/lib/OpenEXRCore/compression.c"
#include "openexr/src/lib/OpenEXRCore/context.c"
#include "openexr/src/lib/OpenEXRCore/chunk.c"
#include "openexr/src/lib/OpenEXRCore/debug.c"
#include "openexr/src/lib/OpenEXRCore/decoding.c"
#include "openexr/src/lib/OpenEXRCore/encoding.c"
#include "openexr/src/lib/OpenEXRCore/float_vector.c"
#include "openexr/src/lib/OpenEXRCore/internal_b44.c"
#include "openexr/src/lib/OpenEXRCore/internal_b44_table.c"
#include "openexr/src/lib/OpenEXRCore/internal_b44_table_init.c"
#include "openexr/src/lib/OpenEXRCore/internal_dwa.c"
#include "openexr/src/lib/OpenEXRCore/internal_dwa_table.c"
#include "openexr/src/lib/OpenEXRCore/internal_dwa_table_init.c"
#include "openexr/src/lib/OpenEXRCore/internal_huf.c"
#include "openexr/src/lib/OpenEXRCore/internal_piz.c"
#include "openexr/src/lib/OpenEXRCore/internal_pxr24.c"
#include "openexr/src/lib/OpenEXRCore/internal_rle.c"
#include "openexr/src/lib/OpenEXRCore/internal_structs.c"
#include "openexr/src/lib/OpenEXRCore/internal_zip.c"
#include "openexr/src/lib/OpenEXRCore/memory.c"
#include "openexr/src/lib/OpenEXRCore/opaque.c"
#include "openexr/src/lib/OpenEXRCore/pack.c"
#include "openexr/src/lib/OpenEXRCore/parse_header.c"
#include "openexr/src/lib/OpenEXRCore/part.c"
#include "openexr/src/lib/OpenEXRCore/part_attr.c"
#include "openexr/src/lib/OpenEXRCore/preview.c"
#include "openexr/src/lib/OpenEXRCore/std_attr.c"
#include "openexr/src/lib/OpenEXRCore/string.c"
#include "openexr/src/lib/OpenEXRCore/string_vector.c"
#include "openexr/src/lib/OpenEXRCore/unpack.c"
#include "openexr/src/lib/OpenEXRCore/validation.c"
#include "openexr/src/lib/OpenEXRCore/write_header.c"

// Our inputs are one-scanline chunks (NONE, RLE, ZIPS), so the HT and zstd codecs are not compiled.
exr_result_t internal_exr_undo_ht(exr_decode_pipeline_t* decode, const void* compressed_data, uint64_t comp_buf_size, void* uncompressed_data, uint64_t uncompressed_size)
{
	return EXR_ERR_FEATURE_NOT_IMPLEMENTED;
}

exr_result_t internal_exr_undo_zstd(exr_decode_pipeline_t* decode, const void* compressed_data, uint64_t comp_buf_size, void* uncompressed_data, uint64_t uncompressed_size)
{
	return EXR_ERR_FEATURE_NOT_IMPLEMENTED;
}

exr_result_t internal_exr_apply_ht(exr_encode_pipeline_t* encode)
{
	return EXR_ERR_FEATURE_NOT_IMPLEMENTED;
}

exr_result_t internal_exr_apply_zstd(exr_encode_pipeline_t* encode)
{
	return EXR_ERR_FEATURE_NOT_IMPLEMENTED;
}

#pragma warning(pop)
