JSON library by Santtu S. Nyman.

Cross-Platform C library without dependencies for parsing and modifying JSON text.
This library does not use any OS services or runtime libraries.
Everything done by this library is purely computational,
so there is no need for C standard library or any OS.

Documentation of functions and data structures of the library are provided in the file "jsonl.h".

The library was originally written only for parsing JSON files.
It was originally used in OAMK storage robot project of class TVT17SPL in 2019
https://blogi.oamk.fi/2019/12/28/projektiryhmien-yhteistyo-kannatti-varastorobo-jarjestelma-ohjaa-lastaa-kuljettaa-ja-valvoo/
The purpose of the library in that project was to load program configuration
for the central server used to receive user request and command the robots.

In 2020 this library was expanded to first print the parsed JSON data back to JSON text and
at the late 2020 functionality for modifying JSON data was added. At this point the library
was significantly different from what it originally was and I decided to rename the library from
JSON parser library (jsonpl) to just JSON library (jsonl), because the original name was conflicting
with it's expanded purpose.
