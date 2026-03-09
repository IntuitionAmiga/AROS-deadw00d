/*
 *	wave.datatype
 *	(c) Fredrik Wikstrom
 */

#include "wave_class.h"
#include "decoders.h"

#ifdef __mc68000__
#include <libraries/iewarp.h>
#include <ie_hwreg.h>

/*
 * IE64 accelerated audio block decoding.
 * Dispatches WARP_OP_AUDIO_DECODE with bulk compressed data.
 * REQ_PTR = compressed source, REQ_LEN = source length in bytes,
 * RESP_PTR = decoded dest, RESP_CAP = codec format tag.
 * Returns number of frames decoded, or 0 on failure.
 */
static LONG ie_decode_blocks(UBYTE *src, UBYTE *dst, LONG src_len, UWORD codec_tag)
{
    ie_write32(IE_COPROC_CPU_TYPE, IE_EXEC_TYPE_IE64);
    ie_write32(IE_COPROC_OP, WARP_OP_AUDIO_DECODE);
    ie_write32(IE_COPROC_REQ_PTR, (ULONG)src);
    ie_write32(IE_COPROC_REQ_LEN, (ULONG)src_len);
    ie_write32(IE_COPROC_RESP_PTR, (ULONG)dst);
    ie_write32(IE_COPROC_RESP_CAP, (ULONG)codec_tag);
    ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_ENQUEUE);

    if (ie_read32(IE_COPROC_CMD_STATUS) == 0)
    {
        ULONG ticket = ie_read32(IE_COPROC_TICKET);
        ie_write32(IE_COPROC_TICKET, ticket);
        ie_write32(IE_COPROC_TIMEOUT, 10000);
        ie_write32(IE_COPROC_CMD, IE_COPROC_CMD_WAIT_CMD);
        if (ie_read32(IE_COPROC_TICKET_STATUS) == IE_COPROC_ST_OK)
            return 1;  /* success — IE64 decoded all blocks */
    }
    return 0;  /* fallback to M68K */
}

#define IE_AUDIO_DECODE_THRESHOLD 1024  /* minimum bytes for IE64 dispatch */
#endif

#include "wave_pcm.h"
#include "wave_ima_adpcm.h"
#include "wave_ms_adpcm.h"
#include "wave_alaw.h"
#include "wave_ieee_float.h"
#include "wave_gsm610.h"
#include "wave_g72x.h"

DECODERPROTO(DecodeBlocks);

static struct Decoder all_decoders[] = {
	{ WAVE_FORMAT_PCM,			0,	SetupPCM,			NULL,			DecodePCM,			NULL			},
	{ WAVE_FORMAT_IMA_ADPCM,	0,	SetupIMA_ADPCM,		NULL,			DecodeBlocks,		DecodeIMA_ADPCM	},
	{ WAVE_FORMAT_ADPCM,		0,	SetupMS_ADPCM,		NULL,			DecodeBlocks,		DecodeMS_ADPCM	},
	{ WAVE_FORMAT_IEEE_FLOAT,	0,	SetupIEEE_Float,	NULL,			NULL,				NULL			},
	{ WAVE_FORMAT_ALAW,			0,	SetupALaw,			NULL,			DecodeALaw,			NULL			},
	{ WAVE_FORMAT_MULAW,		0,	SetupALaw,			NULL,			DecodeMuLaw,		NULL			},
	#ifdef GSM610_SUPPORT
	{ WAVE_FORMAT_GSM610,		0,	SetupGSM610,		CleanupGSM610,	DecodeBlocks,		DecodeGSM610	},
	#endif
	#ifdef G72X_SUPPORT
	{ WAVE_FORMAT_G721_ADPCM,	0,	SetupG72x,			NULL,			DecodeBlocks,		DecodeG72x		},
	{ WAVE_FORMAT_G723_ADPCM,	0,	SetupG72x,			NULL,			DecodeBlocks,		DecodeG72x		},
	#endif
	{ 0 }
};

struct Decoder * GetDecoder (UWORD fmtTag) {
	struct Decoder *dec;
	for (dec = all_decoders; dec->formatTag; dec++) {
		if (dec->formatTag == fmtTag)
			return dec;
	}
	return NULL;
}

DECODERPROTO(DecodeBlocks) {
	LONG status;
	LONG frames_left;
	LONG frames;
	LONG blocksize;

	frames_left = numFrames;
	frames = data->blockFrames;
	blocksize = fmt->blockAlign;

#ifdef __mc68000__
	/* IE64 accelerated bulk decode for large audio blocks */
	{
		LONG total_src_bytes = ((numFrames + frames - 1) / frames) * blocksize;
		if (total_src_bytes >= IE_AUDIO_DECODE_THRESHOLD &&
		    ie_decode_blocks((UBYTE *)Src, (UBYTE *)Dst, total_src_bytes, fmt->formatTag))
		{
			return numFrames;
		}
	}
#endif

	while (frames_left > 0) {
		if (frames_left < frames) frames = frames_left;

		status = data->DecodeFrames(data, fmt, Src, Dst, 1, frames);
		if (status != frames) {
			return numFrames-frames_left+status;
		}
		Src += blocksize;

		frames_left -= frames;
	}
	return numFrames;
}
