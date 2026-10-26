# Stato di questa repository

- Target software: profilo generico per INNO-Hit IH55UWB / IH55UWB4.
- Dispositivo confermato dall'utente: IH55UWB, firmware `04.56.35`, versione webOS Hub visualizzata `7.5.8`.
- La pagina commerciale del produttore indica webOS 5.0; la discrepanza non è risolta, si dà priorità alla versione letta dal televisore.
- Codice app/runtime importato da CSWeb TV commit `1708158f020c268fb1da4eade5238af4cf49ea54`; dettagli in `UPSTREAM.md`.
- Aggiunte locali: opzione CMake `CSWEB_INNOHIT_55`, title profile, auto-aggiunta idempotente della repo doGior, script `tools/build-innohit-55.sh`, job artifact CI. Versione candidata `0.6.1`.
- CI aggiunge un test di caricamento per AnimeUnity di doGior (commit plugin e SHA-256 fissati) nel runtime ARMv7 isolato; questo non sostituisce il test sulla TV né verifica search/playback.
- Test locali eseguiti: suite Python (37 test totali; 13 skip per toolchain/kit/plugin mancanti), `bash -n` sul nuovo script.
- Ambiente di questo checkout: non dispone di CMake, Docker o webOS NDK; l'IPK non è stato compilato qui.
- Osservazione utente: la release CSWeb TV 0.6.0 si installa/avvia sulla IH55UWB, ma i plugin non funzionano. Variante installata, provider specifico, fase di errore e log non ancora raccolti.
- Test eseguiti da questo fork su INNO-Hit: nessuno. I risultati di `docs/FEASIBILITY_DECISION.md` sono test upstream su un LG OLED C1 e non dimostrano compatibilità con webOS Hub.

Prossima diagnosi: distinguere lettura della repository, download del `.cs3`, avvio ART e registrazione del provider; raccogliere `events.log` e `runtime.log` prima del riavvio. Vedi `docs/compatibility.md` e `docs/roadmap.md`.
