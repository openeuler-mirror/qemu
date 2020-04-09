#ifndef QEMU_SYSEMU_RESET_H
#define QEMU_SYSEMU_RESET_H

typedef void QEMUResetHandler(void *opaque);
typedef struct QEMUResetEntry QEMUResetEntry;

QEMUResetEntry *qemu_get_reset_entry(QEMUResetHandler *func, void *opaque);
void qemu_register_reset_after(QEMUResetEntry *entry,
                               QEMUResetHandler *func, void *opaque);
void qemu_register_reset(QEMUResetHandler *func, void *opaque);
void qemu_unregister_reset(QEMUResetHandler *func, void *opaque);
void qemu_devices_reset(void);

#endif
