// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace satrecomp {
class Guest;
void RegisterGeometry();
void ObserveGeometry(Guest &guest);
void GeometryAfterFrame();
void ResetGeometry();
}
