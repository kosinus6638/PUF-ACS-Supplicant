#pragma once

#include <platform.h>
#include <packets.h>
#include <global_defines.h>

#include <stdlib.h>

#define USE_SOFTWARE_PUF
#define SRAM1_BEGIN                 0x3ffe0000

/**
 * Hardware- (and PUF-) specific implementation 
*/
class SRAM_PUF : public puf::PUF {
private:
    uint8_t *sram;
    uint8_t *stable;

public:
    SRAM_PUF();
    puf::MAC puf_to_mac() const;
    puf::MAC get_puf_response(const puf::MAC& challenge) const;
};
