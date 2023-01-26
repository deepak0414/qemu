/*
 * RISC-V translation routines for the Control-Flow Integrity Extension
 *
 * Copyright (c) 2022-2023 Rivos Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

static MemOp mxl_memop(DisasContext *ctx)
{
    switch (get_xl(ctx)) {
    case MXL_RV32:
        return MO_TEUL;

    case MXL_RV64:
        return MO_TEUQ;

    case MXL_RV128:
        return MO_TEUO;

    default:
        g_assert_not_reached();
    }
}

static bool trans_sspop(DisasContext *ctx, arg_sspop *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    /* sspop can only load into x1 or x5. Everything else defaults to zimops */
    if (a->rd != 1 && a->rd != 5) {
        return false;
    }

    /*
     * get data in TCGv using get_gpr
     * get addr in TCGv using gen_helper_csrr on CSR_SSP
     * use some tcg subtract arithmetic (subtract by XLEN) on addr
     * perform ss store on computed address
     */

    TCGv addr = tcg_temp_new();
    int tmp = (get_xl(ctx) == MXL_RV64) ? 8 : 4;
    TCGv_i32 ssp_csr = tcg_constant_i32(CSR_SSP);
    TCGv data = get_gpr(ctx, a->rd, EXT_NONE);
    gen_helper_csrr(addr, cpu_env, ssp_csr);
    tcg_gen_qemu_ld_tl(data, addr, MMU_IDX_SS_ACCESS,
                       mxl_memop(ctx) | MO_ALIGN);

    /*
     * add XLEN/bitwidth to addr, align to XLEN . How do i do that? Is below
     * the right way
     */
    tcg_gen_addi_tl(addr, addr, tmp);
    gen_set_gpr(ctx, a->rd, data);
    gen_helper_csrw(cpu_env, ssp_csr, addr);

    return true;
}

static bool trans_sspush(DisasContext *ctx, arg_sspush *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    /*
     * sspush can only push from x1 or x5. Everything else defaults to zimops
     */
    if (a->rs1 != 1 && a->rs1 != 5) {
        return false;
    }

    /*
     * get data in TCGv using get_gpr
     * get addr in TCGv using gen_helper_csrr on CSR_SSP
     * use some tcg subtract arithmetic (subtract by XLEN) on addr
     * perform ss store on computed address
     */

    TCGv addr = tcg_temp_new();
    int tmp = (get_xl(ctx) == MXL_RV64) ? -8 : -4;
    TCGv_i32 ssp_csr = tcg_constant_i32(CSR_SSP);
    TCGv data = get_gpr(ctx, a->rs1, EXT_NONE);
    gen_helper_csrr(addr, cpu_env, ssp_csr);

    /*
     * subtract XLEN from addr, align to XLEN . How do i do that? Is below the
     * right way
     */
    tcg_gen_addi_tl(addr, addr, tmp);
    tcg_gen_qemu_st_tl(data, addr, MMU_IDX_SS_ACCESS,
                       mxl_memop(ctx) | MO_ALIGN);

    gen_helper_csrw(cpu_env, ssp_csr, addr);

    return true;
}

static bool trans_sschckra(DisasContext *ctx, arg_sschckra *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    gen_helper_sschkra_mismatch(cpu_env);

    return true;
}

static bool trans_ssprr(DisasContext *ctx, arg_ssprr *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    TCGv dest = get_gpr(ctx, a->rd, EXT_NONE);
    TCGv_i32 ssp_csr = tcg_constant_i32(CSR_SSP);
    gen_helper_csrr(dest, cpu_env, ssp_csr);
    gen_set_gpr(ctx, a->rd, dest);

    return true;
}

static bool trans_ssamoswap(DisasContext *ctx, arg_ssamoswap *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    /* If cfi is enabled then, then rd must be != 0 */

    if (a->rd == 0) {
        return false;
    }

    TCGv dest = dest_gpr(ctx, a->rd);
    TCGv src1 = get_address(ctx, a->rs1, 0);
    TCGv src2 = get_gpr(ctx, a->rs2, EXT_NONE);
    MemOp mop = (MO_ALIGN | ((get_xl(ctx) == MXL_RV32) ? MO_TESL : MO_TESQ));

    tcg_gen_atomic_xchg_tl(dest, src1, src2, MMU_IDX_SS_ACCESS, mop);
    gen_set_gpr(ctx, a->rd, dest);
    return true;
}

static bool trans_lpcll(DisasContext *ctx, arg_lpcll *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /*
     * If this is the first instruction of the TB, let the translator
     * know the landing pad requirement was satisfied. No need to bother
     * checking for CFI feature or enablement.
     */

    if (ctx->base.pc_next == ctx->base.pc_first) {
        ctx->fcfi_lp_expected = false;
        /* PC must be 4 byte aligned */
        if (ctx->fcfi_enabled && ((ctx->base.pc_next) & 0x3)) {
            /*
             * misaligned, according to spec we should raise illegal instead
             * of mis-aligned
             */
            gen_exception_illegal(ctx);
        }
    }

    /* forward cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->fcfi_enabled) {
        return false;
    }

    gen_helper_cfi_check_landing_pad(cpu_env, tcg_constant_i32(a->imm_cfi9),
                               tcg_constant_i32(FCFI_LPLL));
    return true;
}

static bool trans_lpcml(DisasContext *ctx, arg_lpcml *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* forward cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->fcfi_enabled) {
        return false;
    }

    gen_helper_cfi_check_landing_pad(cpu_env, tcg_constant_i32(a->imm_cfi8),
                               tcg_constant_i32(FCFI_ML));
    return true;
}

static bool trans_lpcul(DisasContext *ctx, arg_lpcul *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* forward cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->fcfi_enabled) {
        return false;
    }

    gen_helper_cfi_check_landing_pad(cpu_env, tcg_constant_i32(a->imm_cfi8),
                               tcg_constant_i32(FCFI_UL));
    return true;
}

static bool trans_lpsll(DisasContext *ctx, arg_lpsll *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* forward cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->fcfi_enabled) {
        return false;
    }

    gen_helper_cfi_set_landing_pad(cpu_env, tcg_constant_i32(a->imm_cfi9),
                                   tcg_constant_i32(FCFI_LPLL));

    return true;
}

static bool trans_lpsml(DisasContext *ctx, arg_lpsml *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* forward cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->fcfi_enabled) {
        return false;
    }

    gen_helper_cfi_set_landing_pad(cpu_env, tcg_constant_i32(a->imm_cfi8),
                                   tcg_constant_i32(FCFI_ML));

    return true;
}

static bool trans_lpsul(DisasContext *ctx, arg_lpsul *a)
{
    /* cfi only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* forward cfi not enabled, should go to trans_zimops. return false */
    if (!ctx->fcfi_enabled) {
        return false;
    }

    gen_helper_cfi_set_landing_pad(cpu_env, tcg_constant_i32(a->imm_cfi8),
                                   tcg_constant_i32(FCFI_UL));

    return true;
}
