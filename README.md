# Distributed UDP-RDT Database

Este proyecto implementa una base de datos distribuida en red utilizando UDP y RDT (Reliable Data Transfer). Permite realizar operaciones CRUD (Crear, Leer, Actualizar, Eliminar) en una base de datos distribuida.

## Instrucciones de compilación

Para compilar el código, ejecuta el siguiente comando en la terminal:

bash
make

## Iniciar el servidor
Después de compilar el código, inicia el servidor con el siguiente comando:

bash
./server

Luego, inicia varios almacenamientos (storages) con diferentes puertos:

bash
./storage 12350
./storage 12351
./storage 12352
./storage 12353


## Insertar datos desde el archivo Flavor_network2.txt
Para insertar datos rápidamente desde el archivo Flavor_network2.txt, ejecuta el siguiente comando:

bash
. ./Flavor_network2.txt

## Manipular la base de datos distribuida con UDP y RDT
Para interactuar con la base de datos distribuida, ejecuta el cliente con el siguiente comando:

bash
./client
