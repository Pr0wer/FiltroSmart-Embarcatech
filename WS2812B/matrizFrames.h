#ifndef MATRIZNUMS
#define MATRIZNUMS

#include "matriz.h"

// Frames dos números 0 a 9 em caracteres digitais
static const uint8_t temperatura[2][MATRIZ_ROWS][MATRIZ_COLS] = 
{
   {
    {1, 0, 0, 0, 1},
    {1, 1, 0, 0, 1},
    {1, 0, 1, 0, 1}, // N (Natural)
    {1, 0, 0, 1, 1},
    {1, 0, 0, 0, 1}
  },
  {
    {1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0},
    {1, 0, 1, 1, 1}, // G (Gelado)
    {1, 0, 0, 0, 1},
    {1, 1, 1, 1, 1},
  }
};

// Cor dos indicadores de temperatura (em RGB)
static const uint8_t red = 0;
static const uint8_t green = 0;
static const uint8_t blue = 1;

// Indica a temperatura da água em um desenho na matriz RGB
void desenharTemperatura(char valor)
{   
    uint8_t index = 0;
    if (valor == 'G')
    {
      index++;
    }

    // Adicionar na matrix
    desenharFrame(temperatura[index], red, green, blue);
    atualizarMatriz();
}

#endif