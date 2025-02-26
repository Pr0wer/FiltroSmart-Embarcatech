#include "help.h"

// Trata uma dada variação presente em um valor
void tratarVariacao(uint16_t *valor, uint16_t variacao)
{   
    // Caso o valor esteja dentro da faixa de variação
    if (*valor < variacao)
    {   
        // Considerar como alteração nula
        *valor = 0;
    }
}

void limitar(uint8_t *valor, uint8_t minimo, uint8_t maximo)
{
    if (*valor > maximo)
    {
        *valor = maximo;
    }
    else if (*valor < minimo)
    {
        *valor = minimo;
    }
}