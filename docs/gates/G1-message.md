# G1: message for the keyboard player

For the owner to forward (WhatsApp or email) together with the zip from the latest green
CI run (`X50-Patch-Switcher-windows-<sha>.zip`, artifact of the run named in the Decisions
log). Nothing here is sent by Claude.

How the owner gets the zip: `gh run download <run-id> -n X50-Patch-Switcher-windows-<sha>`
or the run page on GitHub, "Artifacts". Send the zip as it is (do not unzip it).

---

Hola! Te mando un programita para probar el X50 desde el notebook. No instala nada y no
cambia nada en el X50: solo le pregunta cosas y guarda las respuestas. Toma uno o dos
minutos.

1. Guarda el zip adjunto, haz clic derecho sobre el y elige **"Extraer todo"**.
2. Conecta el X50 al notebook con el cable USB y enciendelo. Si tienes abierto el editor
   de KORG u otro programa de musica, cierralo.
3. En la carpeta extraida, haz doble clic en **"Diagnose X50"**. Si Windows muestra un
   aviso azul, elige **"Mas informacion"** y luego **"Ejecutar de todas formas"**.
4. Espera a que diga "Listo". Se abre sola una carpeta en el Escritorio que empieza con
   **X50-diagnose**.
5. Comprime esa carpeta (clic derecho, "Enviar a", "Carpeta comprimida") y mandamela.
   Si en vez de "Listo" aparece otro mensaje, mandame igual la carpeta, o una foto de la
   ventana si no se abrio ninguna.

Gracias!

---

## What comes back, and where it goes

- The folder goes to `docs/gates/G1-results/` (report.txt and the .syx files). The .syx
  captures that become fixtures also go to `tests/data/captures/`.
- Read `report.txt` fully before concluding anything. No SysEx reply at all usually means
  the wrong port pair, or another program holding the port; the report lists every
  attempt.
- Then: WinMM port names and probe results into `docs/MIDI.md` ("verified at G1", date),
  whether a whole bank arrived as one reassembled SysEx, and T1.10 (tests that decode the
  captures and match VNL names for unedited slots).
