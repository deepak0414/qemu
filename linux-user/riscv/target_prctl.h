/* No special prctl support required. */
#ifndef RISCV_TARGET_PRCTL_H
#define RISCV_TARGET_PRCTL_H

/*
 * -TARGET_EINVAL: Unsupported/Invalid flag for this architecture
 * -TARGET_EACCES: try to set an already set CFI feature
 * -TARGET_ENOENT: CFI feature is not supported by CPU
 */
static abi_long do_prctl_cfi(CPUArchState *env,
                                 abi_long option, abi_long flag)
{
    if (env_archcpu(env)->cfg.ext_zicfilp) {

        switch (option) {
        case PR_GET_SHADOW_STACK_STATUS:
            abi_ulong bcfi_status = 0;
            if (env->senvcfg & SENVCFG_SSE) {
                bcfi_status |= PR_SHADOW_STACK_ENABLE;
            }
            return copy_to_user(flag, &bcfi_status, sizeof(bcfi_status)) ? \
                   -EFAULT : 0;

        case PR_SET_SHADOW_STACK_STATUS:
            /* if any other bit is set, its invalid param */
            if (flag & ~PR_SHADOW_STACK_ENABLE) {
                return -TARGET_EINVAL;
            }

           if ((flag & PR_SHADOW_STACK_ENABLE)
                && (env->ssp == 0 && !(env->senvcfg & SENVCFG_SSE))) {
                    zicfiss_shadow_stack_alloc(env);
            } else {
                zicfiss_shadow_stack_release(env);
            }
            env->senvcfg |= (flag & PR_SHADOW_STACK_ENABLE) ? SENVCFG_SSE : 0;
            return 0;

        /* locking not implemented (also not needed for qemu-user) yet */
        case PR_LOCK_SHADOW_STACK_STATUS:
            return -TARGET_EINVAL;

        case PR_GET_INDIR_BR_LP_STATUS:
            abi_ulong fcfi_status = 0;
            /* indirect branch tracking is enabled on the task or not */
            if (env->senvcfg & SENVCFG_LPE) {
                fcfi_status |= PR_INDIR_BR_LP_ENABLE;
            }
            return copy_to_user(flag, &fcfi_status, sizeof(fcfi_status)) ? \
                   -EFAULT : 0;

        case PR_SET_INDIR_BR_LP_STATUS:
            /* if any other bit is set, its invalid param */
            if (flag & ~PR_INDIR_BR_LP_ENABLE) {
                return -TARGET_EINVAL;
            }
            /* set or clear branch tracking */
            env->senvcfg |= (flag & PR_INDIR_BR_LP_ENABLE) ? SENVCFG_LPE : 0;
            return 0;

        /* locking not implemented (also not needed for qemu-user) yet */
        case PR_LOCK_INDIR_BR_LP_STATUS:
            return -TARGET_EINVAL;
        }
    }

    return -TARGET_ENOENT;
}

#define do_prctl_cfi do_prctl_cfi

#endif
