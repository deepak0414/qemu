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
        case PR_GET_INDIR_BR_LP_STATUS:
            abi_ulong fcfi_status = 0;
            /* indirect branch tracking is enabled on the task or not */
            fcfi_status |= (env->ufcfien ? PR_INDIR_BR_LP_ENABLE : 0);
            return copy_to_user(flag, &fcfi_status, sizeof(fcfi_status)) ? \
                   -EFAULT : 0;

        case PR_SET_INDIR_BR_LP_STATUS:
            /* if any other bit is set, its invalid param */
            if (flag & ~PR_INDIR_BR_LP_ENABLE) {
                return -TARGET_EINVAL;
            }
            /* set or clear branch tracking */
            env->ufcfien = (flag & PR_INDIR_BR_LP_ENABLE);
            tb_flush(env_cpu(env));
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
