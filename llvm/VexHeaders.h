#ifndef VEXHEADERS_H
#define VEXHEADERS_H
extern "C" {
#include <libvex.h>
#include <libvex_ir.h>
#include <VEX/priv/main_util.h>
#include <VEX/priv/host_generic_regs.h>
#include <VEX/priv/host_amd64_defs.h>
#include <VEX/priv/guest_amd64_defs.h>
#include <libvex_trc_values.h>
#include <libvex_guest_amd64.h>
typedef VexGuestAMD64State VexGuestState;
}
#endif /* VEXHEADERS_H */
