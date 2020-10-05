// Copyright (c) 2020 Emmanuel Arias
#include "include/ppu2.h"

#include <imgui.h>

#include <cassert>

#include "include/cartridge.h"
// TODO check this and fmt include
#include "include/logger.h"

namespace cpuemulator {

Ppu2::Ppu2(const UiConfig& uiConfig) : m_UiConfig{uiConfig} {}

void Ppu2::Update() {
    m_SpriteOutputScreen.Update();

    if (m_UiConfig.m_PpuShowPatternTable0) {
        UpdatePatternTableSprite(m_SpritePatternTables[0], 0, 0);
        m_SpritePatternTables[0].Update();
    }
    if (m_UiConfig.m_PpuShowPatternTable1) {
        UpdatePatternTableSprite(m_SpritePatternTables[1], 1, 0);
        m_SpritePatternTables[1].Update();
    }

    for (int p = 0; p < 8; ++p)  // For each palette
    {
        for (int s = 0; s < 4; ++s)  // For each index
        {
            const int coordX = (p > 3) ? s + 5 : s;
            const int coordY = p % 4;
            m_SpritePalette.SetPixel(coordX, coordY, GetColorFromPalette(p, s));
        }
    }
    m_SpritePalette.Update();
}

void Ppu2::Render() {
    m_SpriteOutputScreen.Render();
    if (m_UiConfig.m_PpuShowPatternTable0) {
        m_SpritePatternTables[0].Render();
    }
    if (m_UiConfig.m_PpuShowPatternTable1) {
        m_SpritePatternTables[1].Render();
    }
    m_SpritePalette.Render();
}

void Ppu2::RenderWidgets() {
    auto GetNametableString = [&](std::size_t nametableId) -> std::string {
        std::string nametableStr = "";

        for (int y = 0; y < 30; ++y) {
            for (int x = 0; x < 32; ++x) {
                nametableStr += fmt::format(
                    "{:02x} ", m_Nametables[nametableId][y * 32 + x]);
            }
            nametableStr += '\n';
        }
        return nametableStr;
    };

    ImGui::Begin("Nametable #0");
    ImGui::Text(GetNametableString(0).c_str());
    ImGui::End();

    ImGui::Begin("Nametable #1");
    ImGui::Text(GetNametableString(1).c_str());
    ImGui::End();
}

uint8_t Ppu2::CpuRead(uint16_t address, bool readOnly) {
    uint8_t data = 0x00;
    switch (address) {
        case 0x0000:  // control
            break;
        case 0x0001:  // mask
            break;
        case 0x0002:  // Status
            data = static_cast<uint8_t>(m_StatusReg.GetRegister() & 0xE0) |
                   static_cast<uint8_t>(m_PpuDataBuffer & 0x1F);
            m_StatusReg.SetField(VERTICAL_BLANK, false);
            m_AddressLatch = 0x00;
            break;
        case 0x0003:  // OAM address
            break;
        case 0x0004:  // OAM data
            data = m_OAMPtr[m_OAMAddress];
            break;
        case 0x0005:  // Scroll
            break;
        case 0x0006:  // PPU address
            break;
        case 0x0007:  // PPU data
            data = m_PpuDataBuffer;
            m_PpuDataBuffer = PpuRead(m_VramAddress.reg);

            if (m_VramAddress.reg > 0x3F00) {
                data = m_PpuDataBuffer;
            }
            m_VramAddress.reg += m_ControlReg.GetField(INCREMENT_MODE) ? 32 : 1;
            break;
        default:
            break;
    }
    return data;
}

void Ppu2::CpuWrite(uint16_t address, uint8_t data) {
    switch (address) {
        case 0x0000:  // control
            m_ControlReg.SetRegister(data);
            m_TramAddress.nametable_x =
                m_ControlReg.GetField(ControlRegisterFields::NAMETABLE_X);
            m_TramAddress.nametable_y =
                m_ControlReg.GetField(ControlRegisterFields::NAMETABLE_Y);
            break;
        case 0x0001:  // mask
            m_MaskReg.SetRegister(data);
            break;
        case 0x0002:  // Status
            break;
        case 0x0003:  // OAM address
            m_OAMAddress = data;
            break;
        case 0x0004:  // OAM data
            m_OAMPtr[m_OAMAddress] = data;
            break;
        case 0x0005:  // Scroll
            if (m_AddressLatch == 0x00) {
                m_FineX = data & 0x07;
                m_TramAddress.coarse_x = data >> 3;
                m_AddressLatch = 0x01;
            } else {
                m_TramAddress.fine_y = data & 0x07;
                m_TramAddress.coarse_y = data >> 3;

                m_AddressLatch = 0x00;
            }
            break;
        case 0x0006:  // PPU address
            if (m_AddressLatch == 0x00) {
                m_TramAddress.reg = (uint16_t)((data & 0x3F) << 8) |
                                    (m_TramAddress.reg & 0x00FF);
                m_AddressLatch = 0x01;
            } else {
                m_TramAddress.reg = (m_TramAddress.reg & 0xFF00) | data;
                m_VramAddress = m_TramAddress;
                m_AddressLatch = 0x00;
            }
            break;
        case 0x0007:  // PPU data
            PpuWrite(m_VramAddress.reg, data);
            m_VramAddress.reg += m_ControlReg.GetField(INCREMENT_MODE) ? 32 : 1;
            break;
        default:
            break;
    }
}

void Ppu2::UpdatePatternTableSprite(Sprite& sprite, unsigned int index,
                                    uint8_t palette) {
    for (uint16_t nTileX = 0; nTileX < 16; ++nTileX) {
        for (uint16_t nTileY = 0; nTileY < 16; ++nTileY) {
            uint16_t offset = nTileY * 256 + nTileX * 16;

            for (uint16_t row = 0; row < 8; ++row) {
                uint8_t tileLSB = PpuRead(index * 0x1000 + offset + row + 0);
                uint8_t tileMSB = PpuRead(index * 0x1000 + offset + row + 8);
                for (uint16_t col = 0; col < 8; ++col) {
                    uint8_t pixel = static_cast<uint8_t>(tileLSB & 0b01) +
                                    static_cast<uint8_t>((tileMSB << 1) & 0b10);
                    tileLSB >>= 1;
                    tileMSB >>= 1;

                    sprite.SetPixel(nTileX * 8 + (7 - col), nTileY * 8 + row,
                                    GetColorFromPalette(palette, pixel));
                }
            }
        }
    }
}

void Ppu2::ConnectCatridge(const std::shared_ptr<Cartridge>& cartridge) {
    Logger::Get().Log("PPU", "Connecting cartridge");
    m_Cartridge = cartridge;
}

void (Ppu2::*Ppu2::m_OnEnterCallbacks[Ppu_States::kStatesSize])(){
    &Ppu2::OnEnter_PrerenderClear,
    &Ppu2::OnEnter_PrerenderTransferY,
    &Ppu2::OnEnter_RenderSkipOdd,
    &Ppu2::OnEnter_RenderProcessNextTile,
    &Ppu2::OnEnter_RenderIncrementScrollY,
    &Ppu2::OnEnter_RenderLoadShiftersAndTransferX,
    &Ppu2::OnEnter_RenderLoadNextBackgroundTile,
    &Ppu2::OnEnter_RenderDoOAMTransfer,
    &Ppu2::OnEnter_RenderUpdateSprites,
    &Ppu2::OnEnter_RenderEndFrameRendering,
};

uint8_t Ppu2::PpuRead(uint16_t address, bool readOnly) {
    uint8_t data = 0x00;
    address &= 0x3FFF;

    if (m_Cartridge->PpuRead(address, data)) {
    } else if (address >= 0x0000 && address <= 0x1FFF) {
        data = m_PatternTables[(address & 0x1000) >> 12][address & 0x0FFF];
    } else if (address >= 0x2000 && address <= 0x3EFF) {
        address &= 0x0FFF;
        if (m_Cartridge->mirror == Cartridge::MIRROR::VERTICAL) {
            // Vertical
            if (address >= 0x0000 && address <= 0x03FF)
                data = m_Nametables[0][address & 0x03FF];
            if (address >= 0x0400 && address <= 0x07FF)
                data = m_Nametables[1][address & 0x03FF];
            if (address >= 0x0800 && address <= 0x0BFF)
                data = m_Nametables[0][address & 0x03FF];
            if (address >= 0x0C00 && address <= 0x0FFF)
                data = m_Nametables[1][address & 0x03FF];
        } else if (m_Cartridge->mirror == Cartridge::MIRROR::HORIZONTAL) {
            // Horizontal
            if (address >= 0x0000 && address <= 0x03FF)
                data = m_Nametables[0][address & 0x03FF];
            if (address >= 0x0400 && address <= 0x07FF)
                data = m_Nametables[0][address & 0x03FF];
            if (address >= 0x0800 && address <= 0x0BFF)
                data = m_Nametables[1][address & 0x03FF];
            if (address >= 0x0C00 && address <= 0x0FFF)
                data = m_Nametables[1][address & 0x03FF];
        }
    } else if (address >= 0x3F00 && address <= 0x3FFF) {
        address &= 0x001F;
        if (address == 0x0010) address = 0x0000;
        if (address == 0x0014) address = 0x0004;
        if (address == 0x0018) address = 0x0008;
        if (address == 0x001C) address = 0x000C;
        data = m_PaletteTable[address];
    }
    return data;
}

void Ppu2::PpuWrite(uint16_t address, uint8_t data) {
    address &= 0x3FFF;
    if (m_Cartridge->PpuWrite(address, data)) {
    } else if (address >= 0x0000 && address <= 0x1FFF) {
        m_PatternTables[(address & 0x1000) >> 12][address & 0x0FFF] = data;
    } else if (address >= 0x2000 && address <= 0x3EFF) {
        address &= 0x0FFF;
        if (m_Cartridge->mirror == Cartridge::MIRROR::VERTICAL) {
            // Vertical
            if (address >= 0x0000 && address <= 0x03FF)
                m_Nametables[0][address & 0x03FF] = data;
            if (address >= 0x0400 && address <= 0x07FF)
                m_Nametables[1][address & 0x03FF] = data;
            if (address >= 0x0800 && address <= 0x0BFF)
                m_Nametables[0][address & 0x03FF] = data;
            if (address >= 0x0C00 && address <= 0x0FFF)
                m_Nametables[1][address & 0x03FF] = data;
        } else if (m_Cartridge->mirror == Cartridge::MIRROR::HORIZONTAL) {
            // Horizontal
            if (address >= 0x0000 && address <= 0x03FF)
                m_Nametables[0][address & 0x03FF] = data;
            if (address >= 0x0400 && address <= 0x07FF)
                m_Nametables[0][address & 0x03FF] = data;
            if (address >= 0x0800 && address <= 0x0BFF)
                m_Nametables[1][address & 0x03FF] = data;
            if (address >= 0x0C00 && address <= 0x0FFF)
                m_Nametables[1][address & 0x03FF] = data;
        }
    } else if (address >= 0x3F00 && address <= 0x3FFF) {
        address &= 0x001F;
        if (address == 0x0010) address = 0x0000;
        if (address == 0x0014) address = 0x0004;
        if (address == 0x0018) address = 0x0008;
        if (address == 0x001C) address = 0x000C;
        m_PaletteTable[address] = data;
    }
}

void Ppu2::UpdateShifters() {
    if (m_MaskReg.GetField(RENDER_BACKGROUND)) {
        m_BackgroundShifter.patternLo <<= 1;
        m_BackgroundShifter.patternHi <<= 1;

        m_BackgroundShifter.attributeLo <<= 1;
        m_BackgroundShifter.attributeHi <<= 1;
    }
    if (m_MaskReg.GetField(RENDER_SPRITES) && m_Cycle >= 0 && m_Cycle < 258) {
        for (int i = 0; i < m_SpriteCount; ++i) {
            if (m_SpriteScanLine[i].x > 0) {
                --m_SpriteScanLine[i].x;
            } else {
                m_SpriteShifterPatternLo[i] <<= 1;
                m_SpriteShifterPatternHi[i] <<= 1;
            }
        }
    }
};

void Ppu2::LoadBackgroundShifters() {
    m_BackgroundShifter.patternLo =
        (m_BackgroundShifter.patternLo & 0xFF00) | m_NextBackgroundTileInfo.lsb;
    m_BackgroundShifter.patternHi =
        (m_BackgroundShifter.patternHi & 0xFF00) | m_NextBackgroundTileInfo.msb;

    m_BackgroundShifter.attributeLo =
        (m_BackgroundShifter.attributeLo & 0xFF00) |
        ((m_NextBackgroundTileInfo.attribute & 0b01) ? 0xFF : 0x00);
    m_BackgroundShifter.attributeHi =
        (m_BackgroundShifter.attributeHi & 0xFF00) |
        ((m_NextBackgroundTileInfo.attribute & 0b10) ? 0xFF : 0x00);
};

void Ppu2::IncrementScrollX() {
    if (m_MaskReg.GetField(MaskRegisterFields::RENDER_BACKGROUND) ||
        m_MaskReg.GetField(MaskRegisterFields::RENDER_SPRITES)) {
        if (m_VramAddress.coarse_x == 31) {
            m_VramAddress.coarse_x = 0;
            m_VramAddress.nametable_x = ~m_VramAddress.nametable_x;
        } else {
            m_VramAddress.coarse_x++;
        }
    }
};

void Ppu2::TransferAddressX() {
    if (m_MaskReg.GetField(MaskRegisterFields::RENDER_BACKGROUND) ||
        m_MaskReg.GetField(MaskRegisterFields::RENDER_SPRITES)) {
        m_VramAddress.nametable_x = m_TramAddress.nametable_x;
        m_VramAddress.coarse_x = m_TramAddress.coarse_x;
    }
};

size_t Ppu2::GetNextState(std::array<Ppu_States, 3>& nextActions) {
    /*
        kPrerenderClear,
        kPrerenderTransferY,
        kRenderSkipOdd,
        kRenderProcessNextTile,
        kRenderIncrementScrollY,
        kRenderLoadShiftersAndTransferX,
        kRenderLoadNextBackgroundTile,
        kRenderDoOAMTransfer,
        kRenderUpdateSprites,
        kRenderEndFrameRendering,
        kStatesSize
        */
    size_t arrIndex = 0;
    if (const bool isPreRenderScanline = m_ScanLine == -1;
        isPreRenderScanline) {
        if (m_Cycle == 1) {
            nextActions[arrIndex++] = Ppu_States::kPrerenderClear;
        } else if (m_Cycle >= 280 && m_Cycle < 305) {
            nextActions[arrIndex++] = Ppu_States::kPrerenderTransferY;
        }
    }
    if (m_ScanLine == 0 && m_Cycle == 0) {
        nextActions[arrIndex++] = Ppu_States::kRenderSkipOdd;
    }
    if (const bool isRenderScanline = m_ScanLine >= -1 && m_ScanLine < 240;
        isRenderScanline) {
        if ((m_Cycle >= 2 && m_Cycle < 258) ||
            (m_Cycle >= 321 && m_Cycle < 338)) {
            nextActions[arrIndex++] = Ppu_States::kRenderProcessNextTile;
        }

        if (m_Cycle == 256) {
            nextActions[arrIndex++] = Ppu_States::kRenderIncrementScrollY;
        }

        if (m_Cycle == 257) {
            nextActions[arrIndex++] =
                
                Ppu_States::kRenderLoadShiftersAndTransferX;
        }

        if (m_Cycle == 338 || m_Cycle == 340) {
            nextActions[arrIndex++] = Ppu_States::kRenderLoadNextBackgroundTile;
        }
        if (m_Cycle == 257 && m_ScanLine >= 0) {
            nextActions[arrIndex++] = Ppu_States::kRenderDoOAMTransfer;
        }
        if (m_Cycle == 340) {
            nextActions[arrIndex++] = Ppu_States::kRenderUpdateSprites;
        }
    }

    if (m_ScanLine == 241 && m_Cycle == 1) {  // covered
        nextActions[arrIndex++] = Ppu_States::kRenderEndFrameRendering;
    }
    return arrIndex;
}

void Ppu2::Clock() {
    // TODO make 3 as a max const
    static std::array<Ppu_States, 3> nextActions;
    size_t actions = GetNextState(nextActions);
    assert(actions <= 3);
    for (size_t i = 0; i < actions; i++) {
        (this->*m_OnEnterCallbacks[static_cast<int>(nextActions[i])])();
    }

    uint8_t bgPixel = 0x00;
    uint8_t bgPalette = 0x00;
    if (m_MaskReg.GetField(MaskRegisterFields::RENDER_BACKGROUND)) {
        uint16_t bitMux = 0x8000 >> m_FineX;

        uint8_t p0_pixel = (m_BackgroundShifter.patternLo & bitMux) > 0;
        uint8_t p1_pixel = (m_BackgroundShifter.patternHi & bitMux) > 0;

        bgPixel = (p1_pixel << 1) | p0_pixel;

        uint8_t bg_pal0 = (m_BackgroundShifter.attributeLo & bitMux) > 0;
        uint8_t bg_pal1 = (m_BackgroundShifter.attributeHi & bitMux) > 0;
        bgPalette = (bg_pal1 << 1) | bg_pal0;
    }

    uint8_t fg_pixel = 0x00;
    uint8_t fg_palette = 0x00;
    uint8_t fg_priority = 0x00;

    if (m_MaskReg.GetField(RENDER_SPRITES)) {
        m_SpriteZeroBeingRendered = false;
        for (uint8_t i = 0; i < m_SpriteCount; ++i) {
            if (m_SpriteScanLine[i].x == 0) {
                uint8_t pixelLo = (m_SpriteShifterPatternLo[i] & 0x80) > 0;
                uint8_t pixelHi = (m_SpriteShifterPatternHi[i] & 0x80) > 0;
                fg_pixel = (pixelHi << 1) | pixelLo;

                fg_palette = (m_SpriteScanLine[i].attribute & 0x03) + 0x04;
                fg_priority = (m_SpriteScanLine[i].attribute & 0x20) == 0;

                if (fg_pixel != 0) {
                    if (i == 0) {
                        m_SpriteZeroBeingRendered = true;
                    }
                    break;
                }
            }
        }
    }

    uint8_t pixel = 0x00;
    uint8_t palette = 0x00;

    if (bgPixel == 0 && fg_pixel == 0) {
        pixel = palette = 0;
    } else if (bgPixel == 0 && fg_pixel > 0) {
        pixel = fg_pixel;
        palette = fg_palette;
    } else if (bgPixel > 0 && fg_pixel == 0) {
        pixel = bgPixel;
        palette = bgPalette;
    } else {
        if (fg_priority) {
            pixel = fg_pixel;
            palette = fg_palette;
        } else {
            pixel = bgPixel;
            palette = bgPalette;
        }

        if (m_SpriteZeroHitPossible && m_SpriteZeroBeingRendered) {
            if (m_MaskReg.GetField(RENDER_BACKGROUND) &
                m_MaskReg.GetField(RENDER_SPRITES)) {
                // The left edge of the screen has specific switches to control
                // its appearance. This is used to smooth inconsistencies when
                // scrolling (since sprites x coord must be >= 0)
                if (!(m_MaskReg.GetField(RENDER_BACKGROUND_LEFT) |
                      m_MaskReg.GetField(RENDER_SPRITES_LEFT))) {
                    if (m_Cycle >= 9 && m_Cycle < 258) {
                        m_StatusReg.SetField(SPRITE_ZERO_HIT, true);
                    }
                } else {
                    if (m_Cycle >= 1 && m_Cycle < 258) {
                        m_StatusReg.SetField(SPRITE_ZERO_HIT, true);
                    }
                }
            }
        }
    }

    m_SpriteOutputScreen.SetPixel(m_Cycle - 1, m_ScanLine,
                                  GetColorFromPalette(palette, pixel));

    ++m_Cycle;
    if (m_Cycle >= 341) {
        m_Cycle = 0;
        ++m_ScanLine;
        if (m_ScanLine >= 261) {
            m_ScanLine = -1;
            isFrameComplete = true;
        }
    }
}

int Ppu2::GetColorFromPalette(uint8_t palette, uint8_t pixel) {
    assert(pixel <= 3);
    uint8_t data = PpuRead(0x3F00 + (palette << 2) + pixel) & 0x3F;

    return m_PalScreen[data];
}

void Ppu2::OnEnter_PrerenderClear() {
    m_StatusReg.SetField(VERTICAL_BLANK, false);

    m_StatusReg.SetField(SPRITE_OVERFLOW, false);

    m_StatusReg.SetField(SPRITE_ZERO_HIT, false);

    for (int i = 0; i < 8; ++i) {
        m_SpriteShifterPatternLo[i] = 0;
        m_SpriteShifterPatternHi[i] = 0;
    }
}

void Ppu2::OnEnter_PrerenderTransferY() {
    if (m_MaskReg.GetField(MaskRegisterFields::RENDER_BACKGROUND) ||
        m_MaskReg.GetField(MaskRegisterFields::RENDER_SPRITES)) {
        m_VramAddress.fine_y = m_TramAddress.fine_y;
        m_VramAddress.nametable_y = m_TramAddress.nametable_y;
        m_VramAddress.coarse_y = m_TramAddress.coarse_y;
    }
}

void Ppu2::OnEnter_RenderSkipOdd() {}

void Ppu2::OnEnter_RenderProcessNextTile() {
    UpdateShifters();

    switch ((m_Cycle - 1) % 8) {
        case 0:
            LoadBackgroundShifters();
            m_NextBackgroundTileInfo.id =
                PpuRead(0x2000 | (m_VramAddress.reg & 0x0FFF));
            break;
        case 2:
            m_NextBackgroundTileInfo.attribute =
                PpuRead(0x23C0 | (m_VramAddress.nametable_y << 11) |
                        (m_VramAddress.nametable_x << 10) |
                        ((m_VramAddress.coarse_y >> 2) << 3) |
                        (m_VramAddress.coarse_x >> 2));
            if (m_VramAddress.coarse_y & 0x02)
                m_NextBackgroundTileInfo.attribute >>= 4;
            if (m_VramAddress.coarse_x & 0x02)
                m_NextBackgroundTileInfo.attribute >>= 2;
            m_NextBackgroundTileInfo.attribute &= 0x03;
            break;
        case 4:
            m_NextBackgroundTileInfo.lsb =
                PpuRead((m_ControlReg.GetField(
                             ControlRegisterFields::PATTERN_BACKGROUND)
                         << 12) +
                        ((uint16_t)m_NextBackgroundTileInfo.id << 4) +
                        (m_VramAddress.fine_y + 0));
            break;
        case 6:
            m_NextBackgroundTileInfo.msb =
                PpuRead((m_ControlReg.GetField(
                             ControlRegisterFields::PATTERN_BACKGROUND)
                         << 12) +
                        ((uint16_t)m_NextBackgroundTileInfo.id << 4) +
                        (m_VramAddress.fine_y + 8));
            break;
        case 7:
            IncrementScrollX();
            break;
    }
}

void Ppu2::OnEnter_RenderIncrementScrollY() {
    if (m_MaskReg.GetField(MaskRegisterFields::RENDER_BACKGROUND) ||
        m_MaskReg.GetField(MaskRegisterFields::RENDER_SPRITES)) {
        if (m_VramAddress.fine_y < 7) {
            m_VramAddress.fine_y++;
        } else {
            m_VramAddress.fine_y = 0;

            if (m_VramAddress.coarse_y == 29) {
                m_VramAddress.coarse_y = 0;
                m_VramAddress.nametable_y = ~m_VramAddress.nametable_y;
            } else if (m_VramAddress.coarse_y == 31) {
                m_VramAddress.coarse_y = 0;
            } else {
                m_VramAddress.coarse_y++;
            }
        }
    }
}

void Ppu2::OnEnter_RenderLoadShiftersAndTransferX() {
    LoadBackgroundShifters();
    TransferAddressX();
}

void Ppu2::OnEnter_RenderLoadNextBackgroundTile() {
    m_NextBackgroundTileInfo.id =
        PpuRead(0x2000 | (m_VramAddress.reg & 0x0FFF));
}

void Ppu2::OnEnter_RenderDoOAMTransfer() {
    std::memset(m_SpriteScanLine, 0xFF, 8 * sizeof(ObjectAttributeEntry));
    m_SpriteCount = 0;

    uint8_t nOAMEntry = 0;
    m_SpriteZeroHitPossible = false;
    while (nOAMEntry < 64 && m_SpriteCount < 9) {
        int16_t diff = ((int16_t)m_ScanLine - (int16_t)m_OAM[nOAMEntry].y);
        if (diff >= 0 &&
            diff < (m_ControlReg.GetField(ControlRegisterFields::SPRITE_SIZE)
                        ? 16
                        : 8)) {
            if (m_SpriteCount < 8) {
                if (nOAMEntry == 0) {
                    m_SpriteZeroHitPossible = true;
                }
                memcpy(&m_SpriteScanLine[m_SpriteCount], &m_OAM[nOAMEntry],
                       sizeof(ObjectAttributeEntry));
                ++m_SpriteCount;
            }
        }
        ++nOAMEntry;
    }
    m_StatusReg.SetField(SPRITE_OVERFLOW, (m_SpriteCount > 8));
}

void Ppu2::OnEnter_RenderUpdateSprites() {
    for (uint8_t i = 0; i < m_SpriteCount; i++) {
        uint16_t sprite_pattern_addr_lo = 0;

        // Determine the memory addresses that contain the byte of
        // pattern data. We only need the lo pattern address, because
        // the hi pattern address is always offset by 8 from the lo
        // address.
        if (!m_ControlReg.GetField(SPRITE_SIZE)) {
            // 8x8 Sprite Mode

            sprite_pattern_addr_lo =
                (m_ControlReg.GetField(PATTERN_SPRITE) << 12) |
                (m_SpriteScanLine[i].id << 4);

            if (!(m_SpriteScanLine[i].attribute & 0x80)) {
                // Sprite is NOT flipped vertically, i.e. normal
                sprite_pattern_addr_lo |= (m_ScanLine - m_SpriteScanLine[i].y);

            } else {
                // Sprite is flipped vertically, i.e. upside down
                sprite_pattern_addr_lo |=
                    (7 - (m_ScanLine - m_SpriteScanLine[i].y));
            }

        } else {
            // 8x16 Sprite Mode
            sprite_pattern_addr_lo = ((m_SpriteScanLine[i].id & 0x01) << 12);
            if (!(m_SpriteScanLine[i].attribute & 0x80)) {
                // Sprite is NOT flipped vertically, i.e. normal
                sprite_pattern_addr_lo |=
                    ((m_ScanLine - m_SpriteScanLine[i].y) & 0x07);
                if (m_ScanLine - m_SpriteScanLine[i].y < 8) {
                    // Reading Top half Tile
                    sprite_pattern_addr_lo |=
                        ((m_SpriteScanLine[i].id & 0xFE) << 4);
                } else {
                    // Reading Bottom Half Tile
                    sprite_pattern_addr_lo |=
                        (((m_SpriteScanLine[i].id & 0xFE) + 1) << 4);
                }
            } else {
                // Sprite is flipped vertically, i.e. upside down
                sprite_pattern_addr_lo |=
                    (7 - (m_ScanLine - m_SpriteScanLine[i].y) & 0x07);
                if (m_ScanLine - m_SpriteScanLine[i].y < 8) {
                    // Reading Top half Tile
                    sprite_pattern_addr_lo |=
                        (((m_SpriteScanLine[i].id & 0xFE) + 1) << 4);
                } else {
                    // Reading Bottom Half Tile
                    sprite_pattern_addr_lo |=
                        ((m_SpriteScanLine[i].id & 0xFE) << 4);
                }
            }
        }

        uint16_t sprite_pattern_addr_hi = sprite_pattern_addr_lo + 8;

        uint8_t sprite_pattern_bits_lo = PpuRead(sprite_pattern_addr_lo);
        uint8_t sprite_pattern_bits_hi = PpuRead(sprite_pattern_addr_hi);

        // If the sprite is flipped horizontally, we need to flip the
        // pattern bytes.
        if (m_SpriteScanLine[i].attribute & 0x40) {
            // This little lambda function "flips" a byte
            // so 0b11100000 becomes 0b00000111. It's very
            // clever, and stolen completely from here:
            // https://stackoverflow.com/a/2602885
            auto flipbyte = [](uint8_t b) -> uint8_t {
                b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
                b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
                b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
                return b;
            };

            sprite_pattern_bits_lo = flipbyte(sprite_pattern_bits_lo);
            sprite_pattern_bits_hi = flipbyte(sprite_pattern_bits_hi);
        }

        // Finally! We can load the pattern into our sprite shift
        // registers ready for rendering on the next scanline
        m_SpriteShifterPatternLo[i] = sprite_pattern_bits_lo;
        m_SpriteShifterPatternHi[i] = sprite_pattern_bits_hi;
    }
}

void Ppu2::OnEnter_RenderEndFrameRendering() {
    m_StatusReg.SetField(StatusRegisterFields::VERTICAL_BLANK, true);
    if (m_ControlReg.GetField(ControlRegisterFields::ENABLE_NMI)) {
        m_DoNMI = true;
    }
}

}  // namespace cpuemulator