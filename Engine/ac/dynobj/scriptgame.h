//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================
#ifndef __AGS_EE_DYNOBJ__SCRIPTGAME_H
#define __AGS_EE_DYNOBJ__SCRIPTGAME_H

#include "ac/dynobj/cc_agsdynamicobject.h"

// Wrapper around script's "Game" struct, managing access to its variables
struct StaticGame : public AGSCCStaticObject
{
    const char *GetType() override { return "Game"; }
    void    WriteInt32(const char *address, intptr_t offset, int32_t val) override;
};

extern StaticGame      GameStaticManager;

#endif // __AGS_EE_DYNOBJ__SCRIPTGAME_H
