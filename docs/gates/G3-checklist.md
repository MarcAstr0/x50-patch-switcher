# G3: beta checklist for the keyboard player

For the owner to forward with the `v0.9.0` beta zip: `X50-Patch-Switcher-windows-0.9.0-08b5d3a.zip`
from CI run 44958148618 (artifacts expire after 30 days). Nothing here is sent by Claude.
The checklist is in Spanish; each step says what the app and the X50's own screen should
show. She ticks each step (sí / no) and adds a note or photo where something differs.
Results go to `docs/gates/G3-results/`.

Also covers G1 if it has not been sent yet: step 2 runs the same diagnose.

---

## Mensaje

Hola! Aquí va la versión de prueba del programa para el X50. No instala nada y no cambia
nada guardado en el X50: solo elige sonidos, como cuando tocas los botones del panel.
Te tomará unos 20 minutos. Si algo no sale como dice la lista, anota el número del paso
y, si puedes, saca una foto de la pantalla. Al final mándame la lista marcada y la
carpeta del paso 2. ¡Gracias!

## Lista

**Preparación**

1. Clic derecho sobre el zip, **"Extraer todo"**. Conecta el X50 por USB y enciéndelo.
   Cierra el editor de KORG u otros programas de música.
2. Doble clic en **"Diagnose X50"** y espera "Listo". Se abre una carpeta
   **X50-diagnose-...** en el Escritorio: guárdala para mandármela al final.
3. Doble clic en **x50-patch-switcher.exe**. Si Windows muestra un aviso azul: "Más
   información" y luego "Ejecutar de todas formas".
   *Debes ver:* arriba a la izquierda un recuadro claro que imita la pantalla del X50, y
   a la derecha **X50 CONNECTED** con un punto celeste.

**Buscar y probar sonidos (vista normal)**

4. Escribe `piano` en el buscador y presiona **Enter**.
   *En el X50 debe aparecer* el primer sonido de la lista (por ejemplo "COMBI 0:Play",
   "Bank A" y el número y nombre). El recuadro del programa muestra lo mismo.
5. Baja con la flecha **↓** por la lista sin presionar Enter.
   *El X50 no debe cambiar* mientras solo marcas filas.
6. Doble clic en un **programa** (dirección que empieza con PROG).
   *En el X50:* "PROG 0:Play" con ese programa. Luego doble clic en una **combinación**
   (COMBI): el X50 cambia solo a modo Combi y muestra esa combinación.
7. En el X50, cambia de programa con sus propios botones.
   *En el programa:* el recuadro de arriba muestra el sonido nuevo en un segundo o menos.
8. Marca la estrella ☆ de dos sonidos. Haz clic en **Favourites** (arriba en la lista de
   la izquierda): deben aparecer esos dos.

**Armar un set**

9. Abajo, presiona **New** y ponle un nombre al set (por ejemplo el nombre del show).
   Arrastra sonidos de la lista a los recuadros 1 a 8. Doble clic en un recuadro para ponerle un nombre (por ejemplo
   "Intro"). Cierra el programa y ábrelo de nuevo: el set debe seguir igual.

**Vista en vivo (lo más importante)**

10. Presiona **F5**. *Debes ver* 8 (o 6) botones grandes que llenan la pantalla, con el
    sonido actual en celeste con borde blanco y la marca **▶ NOW**.
11. Toca o haz clic en cada botón. *En el X50* cambia el sonido cada vez, también entre
    programas y combinaciones. Un recuadro vacío no hace nada.
12. Usa las teclas **1 a 8**, y las flechas **→ / ←** (o tu pedal, si tienes). Cada
    toque avanza un solo botón; si mantienes la tecla apretada, no debe saltar varios.
    En el último botón, → no hace nada.
13. Presiona **Esc una sola vez**: *no debe salir*, solo aparece un aviso abajo. Presiona
    Esc dos veces seguidas (o mantenlo un segundo): vuelve a la vista normal.
14. Vuelve a F5. Presiona **Alt+F4**: *no debe cerrarse* mientras estés en vivo.
15. Con la vista en vivo abierta, cambia a otra ventana (Alt+Tab) y vuelve. Presiona →
    una vez: avanza un solo botón.
16. Deja la vista en vivo abierta sin tocar nada más tiempo que el que tu notebook tarda
    en apagar la pantalla. *La pantalla no debe apagarse.* Sal con Esc Esc: después de
    ese tiempo, la pantalla sí puede apagarse como siempre.
17. Con la vista en vivo abierta, desconecta el cable USB. *Debes ver* **NO X50** en rojo
    arriba a la derecha; el botón encendido se queda como estaba. Vuelve a conectar:
    en unos segundos aparece **X50 CONNECTED** y los botones funcionan otra vez.

**En el escenario (si puedes, en un ensayo)**

18. Desde 1 metro o más, con poca luz: ¿se lee bien el nombre de cada botón? ¿Se ve
    claro cuál está encendido? Anota los nombres que cueste leer.

**Para terminar**

19. Mándame esta lista marcada, tus notas y fotos, y la carpeta del paso 2.

---

## For the foreman when results come back

- Each "no" becomes a T5.5 task with the step number; photos and notes go to
  `docs/gates/G3-results/`.
- Step 2's folder is the G1 data if G1 was not run separately: follow the G1 section of
  `docs/gates/G1-message.md` and T1.10.
- Steps 12, 15, 16 and 17 are the R4 items that only real Windows hardware can confirm
  (Decisions log, R4 entry). Step 18 is the D4 legibility question for the owner.
