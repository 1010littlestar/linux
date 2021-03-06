/*
 * Copyright (c) 2018 HiSilicon Technologies Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "drv_osal_lib.h"
#include "cryp_hash.h"

#ifdef SOFT_SM3_SUPPORT

/*************************** Internal Structure Definition ****************************/
/** \addtogroup      sm3 */
/** @{*/  /** <!-- [sm3]*/

hi_u32 ROTATELEFT(hi_u32 x, hi_u32 n)
{
    n %= 32;

    if (n == 0) {
        return x;
    }

    return x << n | x >> (32 - n);
}

#define P0(x) ((x) ^  ROTATELEFT((x),9)  ^ ROTATELEFT((x),17))
#define P1(x) ((x) ^  ROTATELEFT((x),15) ^ ROTATELEFT((x),23))

#define FF0(x,y,z) ((x) ^ (y) ^ (z))
#define FF1(x,y,z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))

#define GG0(x,y,z) ((x) ^ (y) ^ (z))
#define GG1(x,y,z) (((x) & (y)) | ((~(x)) & (z)))

#define SM3_BLOCK_SIZE            64
#define SM3_W_SIZE                ((SM3_BLOCK_SIZE) + (WORD_WIDTH))
#define SM3_RESULT_SIZE_IN_WORD   ((SM3_RESULT_SIZE) / (WORD_WIDTH))
#define SM3_PAD_MIN_SIZE          9
#define SM3_PAD_LEN_SIZE          8
#define SM3_BYTE_MSB              0x80

/* SM3, the initial hash value, H(0)*/
#define SM3_H0    0x7380166F
#define SM3_H1    0x4914B2B9
#define SM3_H2    0x172442D7
#define SM3_H3    0xDA8A0600
#define SM3_H4    0xA96F30BC
#define SM3_H5    0x163138AA
#define SM3_H6    0xE38DEE4D
#define SM3_H7    0xB0FB0E4E

#define HASH_MAX_BUFFER_SIZE    0x10000 /* 64K */

/**
 * \brief          aes ccm context structure
 */
typedef struct {
    hi_u32 state[SM3_RESULT_SIZE_IN_WORD];
    hi_u8  tail[SM3_BLOCK_SIZE];
    hi_u32 tail_len;
    hi_u32 total;
}
ext_sm3_context;

/** @}*/  /** <!-- ==== Structure Definition end ====*/

/******************************* API Code *****************************/
/** \addtogroup      sm3 */
/** @{*/  /** <!-- [sm3]*/

hi_s32 sm3_compress(hi_u32 digest[SM3_RESULT_SIZE_IN_WORD], const hi_u8 block[SM3_BLOCK_SIZE])
{
    hi_s32 j;
    hi_u32 *W = HI_NULL;
    hi_u32 *W1 = HI_NULL;
    hi_u32 *T = HI_NULL;
    const hi_u32 *local_block = (const hi_u32 *)block;
    hi_u32 A = digest[0];
    hi_u32 B = digest[1];
    hi_u32 C = digest[2];
    hi_u32 D = digest[3];
    hi_u32 E = digest[4];
    hi_u32 F = digest[5];
    hi_u32 G = digest[6];
    hi_u32 H = digest[7];
    hi_u32 SS1, SS2, TT1, TT2;
    hi_u32 *buf = HI_NULL;
    hi_u32 buf_size = 0;

    buf_size = sizeof(hi_u32) * (SM3_W_SIZE + SM3_BLOCK_SIZE + SM3_BLOCK_SIZE);
    buf = (hi_u32 *)crypto_malloc(buf_size);
    if (buf == HI_NULL) {
        HI_LOG_ERROR("sm3 compress crypto malloc buff failed!\n");
        return HI_ERR_CIPHER_FAILED_MEM;
    }
    crypto_memset(buf, buf_size, 0, buf_size);
    W = buf;
    W1 = buf + SM3_W_SIZE;
    T = buf + SM3_W_SIZE + SM3_BLOCK_SIZE;

    for (j = 0; j < 16; j++) {
        W[j] = CPU_TO_BE32(local_block[j]);
    }
    for (j = 16; j < SM3_W_SIZE; j++) {
        W[j] = P1(W[j - 16] ^ W[j - 9] ^ ROTATELEFT(W[j - 3], 15)) ^ ROTATELEFT(W[j - 13], 7) ^ W[j - 6];
    }
    for (j = 0; j < SM3_BLOCK_SIZE; j++) {
        W1[j] = W[j] ^ W[j + 4];
    }

    for (j = 0; j < 16; j++) {

        T[j] = 0x79CC4519;
        SS1 = ROTATELEFT((ROTATELEFT(A, 12) + E + ROTATELEFT(T[j], j)), 7);
        SS2 = SS1 ^ ROTATELEFT(A, 12);
        TT1 = FF0(A, B, C) + D + SS2 + W1[j];
        TT2 = GG0(E, F, G) + H + SS1 + W[j];
        D = C;
        C = ROTATELEFT(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = ROTATELEFT(F, 19);
        F = E;
        E = P0(TT2);
    }

    for (j = 16; j < SM3_BLOCK_SIZE; j++) {

        T[j] = 0x7A879D8A;
        SS1 = ROTATELEFT((ROTATELEFT(A, 12) + E + ROTATELEFT(T[j], j)), 7);
        SS2 = SS1 ^ ROTATELEFT(A, 12);
        TT1 = FF1(A, B, C) + D + SS2 + W1[j];
        TT2 = GG1(E, F, G) + H + SS1 + W[j];
        D = C;
        C = ROTATELEFT(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = ROTATELEFT(F, 19);
        F = E;
        E = P0(TT2);
    }

    digest[0] ^= A;
    digest[1] ^= B;
    digest[2] ^= C;
    digest[3] ^= D;
    digest[4] ^= E;
    digest[5] ^= F;
    digest[6] ^= G;
    digest[7] ^= H;

    if (buf != HI_NULL) {
        crypto_free(buf);
        buf = HI_NULL;
    }

    return HI_SUCCESS;
}

static void sm3_init(ext_sm3_context *ctx)
{
    HI_LOG_FUNC_ENTER();

    ctx->state[0] = SM3_H0;
    ctx->state[1] = SM3_H1;
    ctx->state[2] = SM3_H2;
    ctx->state[3] = SM3_H3;
    ctx->state[4] = SM3_H4;
    ctx->state[5] = SM3_H5;
    ctx->state[6] = SM3_H6;
    ctx->state[7] = SM3_H7;

    HI_LOG_FUNC_EXIT();

    return;
}

static hi_s32 sm3_update(ext_sm3_context *ctx, const hi_u8 *data, hi_u32 data_len)
{
    hi_u32 left = 0;
    hi_s32 ret = HI_FAILURE;

    HI_LOG_FUNC_ENTER();

    ctx->total += data_len;

    if (ctx->tail_len) {
        left = SM3_BLOCK_SIZE - ctx->tail_len;
        if (data_len < left) {
            crypto_memcpy(ctx->tail + ctx->tail_len, left, data, data_len);
            ctx->tail_len += data_len;
            return HI_SUCCESS;
        } else {
            crypto_memcpy(ctx->tail + ctx->tail_len, left, data, left);
            ret = sm3_compress(ctx->state, ctx->tail);
            if (ret != HI_SUCCESS) {
                HI_LOG_PRINT_FUNC_ERR(sm3_compress, ret);
                return ret;
            }

            data += left;
            data_len -= left;
        }
    }

    while (data_len >= SM3_BLOCK_SIZE) {
        ret = sm3_compress(ctx->state, data);
        if (ret != HI_SUCCESS) {
            HI_LOG_PRINT_FUNC_ERR(sm3_compress, ret);
            return ret;
        }

        data += SM3_BLOCK_SIZE;
        data_len -= SM3_BLOCK_SIZE;
    }

    ctx->tail_len = data_len;
    if (data_len) {
        crypto_memcpy(ctx->tail, SM3_BLOCK_SIZE, data, data_len);
    }

    HI_LOG_FUNC_EXIT();

    return HI_SUCCESS;
}

static hi_s32 sm3_final(ext_sm3_context *ctx, hi_u8 *digest)
{
    hi_s32 i = 0;
    hi_s32 ret = HI_FAILURE;
    hi_u32 hash[SM3_RESULT_SIZE_IN_WORD] = {0};

    HI_LOG_FUNC_ENTER();

    ctx->tail[ctx->tail_len] = SM3_BYTE_MSB;

    /* a block is enough */
    if (ctx->tail_len + SM3_PAD_MIN_SIZE <= SM3_BLOCK_SIZE) {
        crypto_memset(ctx->tail + ctx->tail_len + 1, SM3_BLOCK_SIZE - ctx->tail_len - 1, 0,
                      SM3_BLOCK_SIZE - ctx->tail_len - 1);
    } else {
        /* 2 block is request */
        crypto_memset(ctx->tail + ctx->tail_len + 1, SM3_BLOCK_SIZE - ctx->tail_len - 1,
                      0, SM3_BLOCK_SIZE - ctx->tail_len - 1);
        ret = sm3_compress(ctx->state, ctx->tail);
        if (ret != HI_SUCCESS) {
            HI_LOG_PRINT_FUNC_ERR(sm3_compress, ret);
            return ret;
        }
        crypto_memset(ctx->tail, SM3_BLOCK_SIZE, 0, SM3_BLOCK_SIZE);
    }

    /* write 8 bytes fix data length * 8 */
    ctx->tail[SM3_BLOCK_SIZE - 5] = (hi_u8)((ctx->total >> 29) & 0x07);
    ctx->tail[SM3_BLOCK_SIZE - 4] = (hi_u8)((ctx->total >> 21) & 0xff);
    ctx->tail[SM3_BLOCK_SIZE - 3] = (hi_u8)((ctx->total >> 13) & 0xff);
    ctx->tail[SM3_BLOCK_SIZE - 2] = (hi_u8)((ctx->total >> 5)  & 0xff);
    ctx->tail[SM3_BLOCK_SIZE - 1] = (hi_u8)((ctx->total << 3)  & 0xff);

    ret = sm3_compress(ctx->state, ctx->tail);
    if (ret != HI_SUCCESS) {
        HI_LOG_PRINT_FUNC_ERR(sm3_compress, ret);
        return ret;
    }

    for (i = 0; i < SM3_RESULT_SIZE_IN_WORD; i++) {
        hash[i] = CPU_TO_BE32(ctx->state[i]);
    }

    crypto_memcpy(digest, SM3_RESULT_SIZE, hash, SM3_RESULT_SIZE);

    HI_LOG_FUNC_EXIT();

    return HI_SUCCESS;
}

void *ext_sm3_create(hash_mode mode)
{
    ext_sm3_context *ctx = HI_NULL;

    HI_LOG_FUNC_ENTER();

    ctx = crypto_malloc(sizeof(ext_sm3_context));
    if (ctx == HI_NULL) {
        HI_LOG_ERROR("malloc hash context buffer failed!");
        return HI_NULL;
    }
    crypto_memset(ctx, sizeof(ext_sm3_context), 0, sizeof(ext_sm3_context));

    sm3_init(ctx);

    HI_LOG_FUNC_EXIT();

    return ctx;
}

hi_s32 ext_sm3_update(void *ctx, hi_u8 *chunk, hi_u32 chunkLen, hash_chunk_src src)
{
    hi_u8 *ptr = HI_NULL;
    hi_s32 ret = HI_FAILURE;
    hi_u32 offset = 0, length = 0;

    HI_LOG_FUNC_ENTER();

    HI_LOG_CHECK_PARAM(ctx == HI_NULL);

    if (chunkLen == 0x00) {
        return HI_SUCCESS;
    }

    if (src == HASH_CHUNCK_SRC_USER) {
        ptr = crypto_malloc(HASH_MAX_BUFFER_SIZE);
        if (HI_NULL == ptr) {
            HI_LOG_ERROR("malloc hash chunk buffer failed!");
            HI_LOG_PRINT_ERR_CODE(HI_ERR_CIPHER_FAILED_MEM);
            return HI_ERR_CIPHER_FAILED_MEM;
        }

        while (offset < chunkLen) {
            length = chunkLen - offset;
            if (length > HASH_MAX_BUFFER_SIZE) {
                length = HASH_MAX_BUFFER_SIZE;
            }
            ret = crypto_copy_from_user(ptr, chunk + offset, length);
            if (ret != HI_SUCCESS) {
                HI_LOG_ERROR("copy hash chunk from user failed!");
                HI_LOG_PRINT_ERR_CODE(HI_ERR_CIPHER_FAILED_MEM);
                goto exit;
            }
            ret = sm3_update(ctx, ptr, length);
            if (ret != HI_SUCCESS) {
                HI_LOG_PRINT_FUNC_ERR(sm3_update, ret);
                goto exit;
            }

            crypto_msleep(1);
            offset += length;
        }
    } else {
        if (chunk == HI_NULL) {
            HI_LOG_ERROR("Invalid point,chunk is null!\n");
            ret = HI_ERR_CIPHER_INVALID_POINT;
            goto exit;
        }
        ret = sm3_update(ctx, chunk, chunkLen);
        if (ret != HI_SUCCESS) {
            HI_LOG_PRINT_FUNC_ERR(sm3_update, ret);
            goto exit;
        }

        ret = HI_SUCCESS;
    }

exit:
    if (ptr != HI_NULL) {
        crypto_free(ptr);
        ptr = HI_NULL;
    }

    HI_LOG_FUNC_EXIT();

    return ret;
}

hi_s32 ext_sm3_finish(void *ctx,  void *hash, hi_u32 *hashlen)
{
    hi_s32 ret = HI_FAILURE;
    HI_LOG_FUNC_ENTER();

    HI_LOG_CHECK_PARAM(ctx == HI_NULL);

    ret = sm3_final(ctx, hash);
    if (ret != HI_SUCCESS) {
        HI_LOG_PRINT_FUNC_ERR(sm3_final, ret);
        return ret;
    }
    *hashlen = SM3_RESULT_SIZE;

    HI_LOG_FUNC_EXIT();
    return HI_SUCCESS;
}

hi_s32 ext_sm3_destory(void *ctx)
{
    HI_LOG_FUNC_ENTER();

    HI_LOG_CHECK_PARAM(ctx == HI_NULL);

    crypto_free(ctx);
    ctx  = HI_NULL;

    HI_LOG_FUNC_EXIT();
    return HI_SUCCESS;
}

#endif
/** @}*/  /** <!-- ==== API Code end ====*/
