/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Minikin"

#include <cutils/log.h>
#include <stdlib.h>
#include <stdint.h>

#include "MinikinInternal.h"
#include <minikin/MinikinFont.h>
#include <minikin/AnalyzeStyle.h>
#include <minikin/FontFamily.h>
#include <UniquePtr.h>

using std::vector;

namespace android {

// Parse bcp-47 language identifier into internal structure
FontLanguage::FontLanguage(const char* buf, size_t size) {
    uint32_t bits = 0;
    size_t i;
    for (i = 0; i < size && buf[i] != '-' && buf[i] != '_'; i++) {
        uint16_t c = buf[i];
        if (c == '-' || c == '_') break;
    }
    if (i == 2) {
        bits = (uint8_t(buf[0]) << 8) | uint8_t(buf[1]);
    }
    size_t next;
    for (i++; i < size; i = next + 1) {
        for (next = i; next < size; next++) {
            uint16_t c = buf[next];
            if (c == '-' || c == '_') break;
        }
        if (next - i == 4 && buf[i] == 'H' && buf[i+1] == 'a' && buf[i+2] == 'n') {
            if (buf[i+3] == 's') {
                bits |= kHansFlag;
            } else if (buf[i+3] == 't') {
                bits |= kHantFlag;
            }
        }
        // TODO: this might be a good place to infer script from country (zh_TW -> Hant),
        // but perhaps it's up to the client to do that, before passing a string.
    }
    mBits = bits;
}

int FontLanguage::match(const FontLanguage other) const {
    int result = 0;
    if ((mBits & kBaseLangMask) == (other.mBits & kBaseLangMask)) {
        result++;
        if ((mBits & kScriptMask) != 0 && (mBits & kScriptMask) == (other.mBits & kScriptMask)) {
            result++;
        }
    }
    return result;
}

FontFamily::~FontFamily() {
    for (size_t i = 0; i < mFonts.size(); i++) {
        mFonts[i].typeface->UnrefLocked();
    }
}

bool FontFamily::addFont(MinikinFont* typeface) {
    AutoMutex _l(gMinikinLock);
    const uint32_t os2Tag = MinikinFont::MakeTag('O', 'S', '/', '2');
    size_t os2Size = 0;
    bool ok = typeface->GetTable(os2Tag, NULL, &os2Size);
    if (!ok) return false;
    UniquePtr<uint8_t[]> os2Data(new uint8_t[os2Size]);
    ok = typeface->GetTable(os2Tag, os2Data.get(), &os2Size);
    if (!ok) return false;
    int weight;
    bool italic;
    if (analyzeStyle(os2Data.get(), os2Size, &weight, &italic)) {
        //ALOGD("analyzed weight = %d, italic = %s", weight, italic ? "true" : "false");
        FontStyle style(weight, italic);
        addFontLocked(typeface, style);
        return true;
    } else {
        ALOGD("failed to analyze style");
    }
    return false;
}

void FontFamily::addFont(MinikinFont* typeface, FontStyle style) {
    AutoMutex _l(gMinikinLock);
    addFontLocked(typeface, style);
}

void FontFamily::addFontLocked(MinikinFont* typeface, FontStyle style) {    typeface->RefLocked();
    mFonts.push_back(Font(typeface, style));
}

// Compute a matching metric between two styles - 0 is an exact match
int computeMatch(FontStyle style1, FontStyle style2) {
    if (style1 == style2) return 0;
    int score = abs(style1.getWeight() - style2.getWeight());
    if (style1.getItalic() != style2.getItalic()) {
        score += 2;
    }
    return score;
}

MinikinFont* FontFamily::getClosestMatch(FontStyle style) const {
    const Font* bestFont = NULL;
    int bestMatch = 0;
    for (size_t i = 0; i < mFonts.size(); i++) {
        const Font& font = mFonts[i];
        int match = computeMatch(font.style, style);
        if (i == 0 || match < bestMatch) {
            bestFont = &font;
            bestMatch = match;
        }
    }
    return bestFont == NULL ? NULL : bestFont->typeface;
}

size_t FontFamily::getNumFonts() const {
    return mFonts.size();
}

MinikinFont* FontFamily::getFont(size_t index) const {
    return mFonts[index].typeface;
}

FontStyle FontFamily::getStyle(size_t index) const {
    return mFonts[index].style;
}

}  // namespace android
