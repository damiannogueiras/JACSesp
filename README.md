# JACS

## Just Another Control Set - esp8266 part


Servidor OSC.

Esp8266 en modo AP, el cliente tiene que conectarse a su red.

Recibe los mensages de los clientes y se los pasa via serie al arduino:

/upload : carga data/upload.html form para upload csv
/mega   : parsea csv y por cada linea manda un bundle al mega, direccion osc /frame

Tiene un led tricolor para indicar el estado:

- ROJO: arrancando y no preparado

- VERDE: listo para recibir

- AZUL: Recibiendo mensaje
