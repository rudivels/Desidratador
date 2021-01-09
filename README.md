# ESP12_Secador
Projeto de desenvolver um desidratador de laboratorio a partir de um forno elétrico.

Desenvolvido por Rudi van Els [site](http://fga.unb.br/rudi.van) 
 
`/Arduino/ESP12_Secador`


# 1. Apresentação 

Por que fazer este desidratador

Especificações do desidratador.
Registrar os seguintes dados durante o processo de secagem a cada 10 segundos de forma automática:

- Temperatura ambiente 
- Umidade relativa do ar no ambiente
- Temperatura no forno
- Temperatura na saída do ar do forno
- Umidade relativa na saída de ar do forno
- Peso do material no forno

Armazenar estes dados medidos durante o processo de secagem e permitir o acesso a estes dados diretamento pela internet em tempo real.

O desidratar tem que permitir controlar

- potência aplicado no resistência de aquecimento
- Velocidade do ventilador de entrada do ar

Estes controles devem ser disponibilizados localmente nos botões de operação do desidratodor, mais também por um comando via internet.

Diagrama de bloco 

Num futuro pretende-se que possa-se estabelecer uma taxa de secagem o o sistema automaticamente fará a secagem segundo essa taxa.

# 2. Preparação

Este video mostra o forno e a proposta de transformar o forno num desidratador. [link para o video no youtube](https://www.youtube.com/watch?v=agu5u9XPCK0)



# 3. Implementação


## 3.1. Balança eletrônica
Pesagem automática. 

[Video com o protótipo da pesagem automática](https://www.youtube.com/watch?v=mP0JLjlJqJM)


## 3.2. Duto de ar de entrada

[Video com a apresentação do duto de ar de entrada ](https://www.youtube.com/watch?v=D7OGNmsQnvQ)

### Medição de umidade e temperatura no duto de entrada

A foto mostra o medidro de umidade e temperatura DHT11 instalada no duto de entrada do ar.
 



