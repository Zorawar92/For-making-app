# Stato di questa repository

- Target software: profilo generico per INNO-Hit IH55UWB / IH55UWB4.
- Dispositivo confermato dall'utente: IH55UWB, firmware `04.56.35`, versione webOS Hub visualizzata `7.5.8`.
- La pagina commerciale del produttore indica webOS 5.0; la discrepanza non è risolta, si dà priorità alla versione letta dal televisore.
- Codice app/runtime importato da CSWeb TV commit `1708158f020c268fb1da4eade5238af4cf49ea54`; dettagli in `UPSTREAM.md`.
- Aggiunte locali: opzione CMake `CSWEB_INNOHIT_55`, title profile, script `tools/build-innohit-55.sh`, job artifact CI.
- Test locali eseguiti: suite Python del progetto upstream (34 test totali; 13 skip per toolchain/kit/plugin mancanti), `bash -n` sul nuovo script.
- Ambiente di questo checkout: non dispone di CMake, Docker o webOS NDK; l'IPK non è stato compilato qui.
- Test su INNO-Hit: nessuno. I risultati di `docs/FEASIBILITY_DECISION.md` sono test upstream su un LG OLED C1 e non dimostrano compatibilità con webOS Hub.

La compatibilità dei due codici INNO-Hit e l'architettura kernel/userspace devono essere verificate con l'IPK su hardware reale prima di dichiarare una release stabile. Vedi `docs/roadmap.md`.
