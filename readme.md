# Implementação Rede CAN

Este programa implementa uma rede CAN para um protótipo de veículo elétrico.
O programa recebe sinais analógicos dos 2 TPSs (sensores) do acelerador e do sensor
de freio, testa a consistência desses sinais e envia o sinal do TPS1, do freio e 
um sinal de estado, indicando se os sinais recebidos estão consistentes, pela rede CAN.
Além disso recebe através da rede CAN a velocidade do carro e envia para os dois displays
do painel o seu valor fazendo uma multiplexação dos dois displays. O programa também
é responsável pelo envio do sinal da luz de freio do carro.

- Programa utilizado em um PIC18f4585