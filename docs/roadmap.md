# Roadmap e gate di fattibilità

Stato: l'utente ha installato l'IPK candidato 0.6.1 e conferma che l'app si avvia su IH55UWB. La grafica non coincide con CloudStream Android e la repo doGior/plugin non ha ancora funzionato. L'IPK con i bundle locali selezionati è in sviluppo e non è ancora testato sulla TV.

## Target generico della famiglia 55″

La scheda del produttore elenca i codici 55″ **IH55UWB** e **IH55UWB4** con specifiche hardware simili. Il proprietario ha confermato il modello **IH55UWB**, firmware `04.56.35` e versione webOS Hub `7.5.8`; la scheda commerciale indica invece webOS 5.0. Registriamo la versione visibile sul televisore come dato osservato e teniamo nota della discrepanza.

Il profilo CMake `CSWEB_INNOHIT_55=ON` produce un singolo IPK con runtime ARMv7 e `libcurl` incluso. È una scelta conservativa in assenza di una lettura di `uname -m` e dell'inventario delle librerie del firmware, non una deduzione dal numero di versione. Il processore ARM Cortex-A55 supporta capacità ARMv8, ma questo **non dimostra** l'ABI kernel/userspace effettivamente installata. Il profilo resta sperimentale finché non è provato su IH55UWB e IH55UWB4.

### Gate prima della release stabile

- Installare e avviare l'IPK su un IH55UWB e su un IH55UWB4, se entrambi disponibili.
- Verificare architettura kernel/userspace, permission/launcher, memoria, HTTP/TLS, database DB8 e uscita audio/video.
- Provare con telecomando: primo avvio, aggiunta repo, installazione plugin, ricerca, dettagli e playback.
- Registrare modello, firmware, log sanitizzati, tempi e RSS; non includere seriali o token.
- Se i due modelli non hanno lo stesso comportamento/ABI, introdurre feature detection o pacchetti distinti, senza affermare una compatibilità generica.

## Gate 0 — inventario e benchmark

- La base nativa è `sostk/cloudstream-webos-native` al commit indicato in `UPSTREAM.md`; `docs/FEASIBILITY_DECISION.md` contiene evidenze upstream su un LG OLED C1, non test eseguiti su questa INNO-Hit. Lo stato di questo fork è in `docs/STATUS.md`.
- Confrontare le revisioni di kit/runtime e mantenere licenze e avvisi di origine.
- Waydroid è un riferimento per il packaging di un ambiente Android completo, ma non fa parte della build e non è richiesto in esecuzione.

## Gate 1 — package e runtime

- Configurare l'NDK webOS e compilare con `tools/build-innohit-55.sh`.
- Verificare l'IPK con `check-package` e annotare commit/hash dell'output.
- Non distribuire l'IPK finché gli avvisi runtime in `runtime/licenses-armv7/` e le licenze dei componenti sono verificati.

## Gate 2 — bundle doGior, provider e playback

- Build personale opt-in: includere StreamingCommunity, AnimeUnity e AnimeWorld pre-scaricati, verificarne hash e importarli/abilitarli al primo avvio. I file `.cs3` non sono committati e non entrano nelle Release pubbliche.
- Provare i tre plugin singolarmente sulla IH55UWB: caricamento, ricerca, dettagli, estrazione link e riproduzione sono gate distinti; raccogliere errore e log del runtime.
- Rendere note le limitazioni dei plugin che richiedono WebView, UI Android/AndroidX, DRM o servizi non disponibili.
- La UI nativa webOS è stata confermata avviabile ma non visivamente equivalente a CloudStream Android; raccogliere screenshot prima di pianificare le modifiche grafiche.

## Gate 3 — rilascio

- Build CI, checksums, documentazione d'installazione e matrice compatibilità per entrambi i codici modello.
- Distinguere sempre test host, package check e test effettivamente condotti su TV.
- Un IPK compilato o installabile non è prova che l'API webOS Hub del dispositivo supporti l'app.
