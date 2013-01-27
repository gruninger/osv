#ifndef PCI_DEVICE_H
#define PCI_DEVICE_H

#include "types.hh"
#include "pci.hh"
#include "pci-function.hh"

namespace pci {

    class pci_device: public pci_function {
    public:
        pci_device(u8 bus, u8 device, u8 func);
        virtual ~pci_device();

        // Parse configuration space of pci_device
        virtual bool parse_pci_config(void);

        u16 get_subsystem_id(void);
        u16 get_subsystem_vid(void);

        virtual void dump_config(void);

    protected:

        u16 _subsystem_vid;
        u16 _subsystem_id;

    };


}

#endif