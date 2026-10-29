# Stato di questa repository

- Target software: profilo generico per INNO-Hit IH55UWB / IH55UWB4.
- Dispositivo confermato dall'utente: IH55UWB, firmware `04.56.35`, versione webOS Hub visualizzata `7.5.8`; l'utente ha confermato che la TV non è rooted.
- ATL completo per webOS non è una strada per questo dispositivo: il fork WebOS-ATL richiede root, aarch64 e ha test dichiarati solo su LG 43UR7500. Strategia scelta: app UI nativa webOS; valutare API/ART ATL solo come riferimento per una fase futura dei plugin.
- La pagina commerciale del produttore indica webOS 5.0; la discrepanza non è risolta, si dà priorità alla versione letta dal televisore.
- Codice app/runtime importato da CSWeb TV commit `1708158f020c268fb1da4eade5238af4cf49ea54`; dettagli in `UPSTREAM.md`.
- Aggiunte locali: opzione CMake `CSWEB_INNOHIT_55`, title profile e script `tools/build-innohit-55.sh`. Prossima versione candidata `0.6.3`, distinta dall'IPK 0.6.1 già pubblicato.
- L'utente ha confermato che il pacchetto candidato si avvia sulla IH55UWB; riferisce che la grafica differisce dall'app Android e che i plugin doGior non funzionano. L'interfaccia del port è nativa webOS.
- Il build locale include per default StreamingCommunity, AnimeUnity e AnimeWorld, fissati a una revisione del ramo `builds`, importati e abilitati al primo avvio; si può escluderli con `CSWEB_BUNDLE_DOGIOR_PLUGINS=0`. Un marker esplicito abilita il bundle solo nell'IPK CI personale. I `.cs3` restano fuori da Git e dalle Release pubbliche perché la repo non dichiara una licenza.
- Nessun plugin doGior è ancora stato testato sulla IH55UWB. La run CI 40842859659 ha isolato il mancato avvio: CloudStream `RequestsHelperKt.buildDefaultClient` chiede `Context.getString(R.string.dns_key)`, ma l'HeadlessContext non aveva risorse applicative e ART terminava con `Resources$NotFoundException` prima di `ready`. Una shim restituisce `dns_key` per l'ID dell'APK CloudStream fissato in `runtime/inputs.lock`.
- Verifica CI 40843367215: il runtime ARMv7 emette `ready` e tutti e tre i plugin selezionati si caricano e registrano provider. L'IPK legacy 0.6.3 con bundle e runtime corretto è stato generato e controllato. Il test è offline e non copre ricerca/rete/playback, né prova la TV; CORS non si applica alle richieste HTTP native, mentre anti-bot e TLS restano possibili limiti.
- La suite e il packaging devono essere rieseguiti dopo questa modifica. Non è stato creato né verificato un IPK con plugin per la nuova build.
- Ambiente di questo checkout: dispone di g++, ma non di CMake, Docker o webOS NDK; l'IPK va compilato dal CI, che ora supporta il marker per il bundle personale.
- I risultati in `docs/FEASIBILITY_DECISION.md` sono prove upstream su LG OLED C1, non su webOS Hub INNO-Hit.

Prossima diagnosi: distinguere lettura della repository, download del `.cs3`, avvio ART e registrazione del provider; raccogliere `events.log` e `runtime.log` prima del riavvio. Vedi `docs/compatibility.md` e `docs/roadmap.md`.
