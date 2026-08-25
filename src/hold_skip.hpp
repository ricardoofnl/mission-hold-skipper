#pragma once

namespace mhs::hold_skip {

// called from both hooks, only the first caller in a frame does the work
void TickOncePerFrame();

bool ConsumeCompleted();

void Draw();

} // namespace mhs::hold_skip
