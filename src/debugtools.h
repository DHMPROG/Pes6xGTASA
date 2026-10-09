// Outils de rétro-ingénierie (F11 / F12)
#pragma once

void Debug_DrawPanel();                                // à appeler entre NewFrame et Render
void Debug_Dump(const char* gameDir, const char* note);  // copie .data dans <jeu>\pes6fun_dumps
