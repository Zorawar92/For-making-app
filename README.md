# CloudStream webOS Hub — INNO-Hit 55 profile

Port nativo per webOS Hub, basato sul progetto community [CSWeb TV](https://github.com/sostk/cloudstream-webos-native). L'app usa UI e player nativi webOS e un runtime locale separato per i plugin CloudStream compatibili: **non installa APK Android e non avvia una sessione Waydroid**. Waydroid è usato come riferimento per separazione e packaging del runtime, non come container da incorporare nella TV. Dettagli in [architettura](docs/architecture.md) e [provenienza del codice](UPSTREAM.md).

## Profilo INNO-Hit 55″

Il profilo `CSWEB_INNOHIT_55` è pensato per **IH55UWB** e **IH55UWB4**, i due codici INNO-Hit da 55″ con specifiche hardware pubblicate simili. Il proprietario ha confermato **IH55UWB**, firmware **04.56.35** e versione webOS Hub **7.5.8**. La scheda commerciale INNO-Hit riporta webOS 5.0, quindi c'è una discrepanza nella numerazione: per il dispositivo usiamo come dato osservato la versione mostrata dal menu TV.

Il profilo crea **un singolo IPK** con runtime ARMv7 e `libcurl` incluso: una scelta prudenziale finché non conosciamo ABI del kernel e disponibilità delle librerie sul firmware Hub. Non è una prova di compatibilità né una garanzia per IH55UWB4; l'IPK va installato e testato su entrambi. Un modello con hardware/firmware differente potrebbe richiedere un profilo distinto.

### Build locale

Il percorso completo richiede Linux, Docker, il webOS NDK e tempo/spazio per compilare il runtime ART ARMv7 e i componenti multimediali. Imposta `WEBOS_NDK` come descritto in [docs/legacy-build.md](docs/legacy-build.md), poi esegui:

```sh
tools/build-innohit-55.sh
```

Il target package è `org.csweb.tv.legacy` versione candidata **0.6.3**; il titolo visualizzato nell'app è **CloudStream Hub - INNO-Hit 55**. Il file IPK viene creato sotto `external/webos-native-app-kit/build/webos-innohit55/dist/`. Il build locale include per default StreamingCommunity, AnimeUnity e AnimeWorld, già installati e abilitati al primo avvio. Lo script scarica i tre `.cs3` dalla revisione doGior fissata nel tool, ne verifica hash e formato e li passa al package builder. I file restano nell'area locale di build e non vengono aggiunti a Git. Per un build locale senza plugin, imposta `CSWEB_BUNDLE_DOGIOR_PLUGINS=0`.

Il CI include i `.cs3` soltanto nei build personali esplicitamente marcati `[build-innohit-plugins]`; i package destinati a Release pubbliche non li includono finché i permessi di redistribuzione non sono verificati. Se l'app trova già una versione di uno di questi plugin nello storage TV, non la sovrascrive. La prova CI ARMv7 ha caricato e registrato i tre provider in un runtime isolato/offline: non dimostra ricerca via rete, playback o compatibilità con webOS Hub. I job Legacy sono facoltativi nei build ordinari; quando il commit richiede una release, diventano obbligatori e un errore blocca la pubblicazione.

### Release GitHub

Un commit su `main` che contiene entrambi i marker `[build-innohit]` e `[release-innohit]` avvia i job ARMv7/webOS per il profilo INNO-Hit; al termine, il workflow **Release latest INNO-Hit IPK** pubblica una pre-release `v<versione>` usando l'IPK prodotto dalla stessa run CI. Controlla il nome/versione del package e gli avvisi runtime, calcola SHA-256 e non recupera artifact da run precedenti. Host-debug, il runtime e il package ARMv7 sono bloccanti. Nella sola pre-release automatica, host-ASan e il probe plugin-load ARMv7 restano visibili ma non bloccano; ASan è obbligatorio nelle pull request, mentre il probe è obbligatorio nella pubblicazione manuale. La release si ferma se la run CI o il package non passano i controlli bloccanti. Il package generico ARM64 si compila separatamente con `[build-webos]`; l'azione manuale **Actions → Release** ricostruisce e pubblica entrambi i profili. Per le Release pubbliche INNO-Hit sono intenzionalmente esclusi i `.cs3` finché la licenza upstream non autorizza chiaramente la redistribuzione.

È disponibile anche **Actions → Release** per una pubblicazione manuale da `main`: inserisci la versione, e il workflow ricostruisce e testa i package prima di creare la release. Non allega un IPK storico o un bundle plugin personale.

### Repo doGior e compatibilità

Il manifest doGior è `https://raw.githubusercontent.com/doGior/doGiorsHadEnough/refs/heads/builds/repo.json`; punta a `plugins.json`, che a sua volta elenca i `.cs3`, le versioni e gli hash. L'app legge questo formato CloudStream e carica i plugin con il runtime ART locale. Un `.cs3` è bytecode Android, non un eseguibile webOS: la registrazione del provider, la ricerca e la riproduzione sono verifiche separate e alcune API Android (per esempio WebView/UI Android o DRM) possono non essere supportate.

Le richieste dei plugin girano nel runtime locale sulla TV; quelle del client dell'app passano dal trasporto HTTP nativo webOS. Non sono richieste `fetch` di una pagina browser, quindi CORS non è il limite e non serve un bridge su un altro dispositivo. DNS, TLS, blocchi anti-bot/Cloudflare e cambiamenti dei siti possono comunque impedire una richiesta o l'estrazione di un link.

Il bundle personale citato sopra fissa una revisione doGior e verifica l'integrità dei file, ma **non certifica che quei plugin funzionino sulla TV**. I `.cs3` non sono redistribuiti nelle Release pubbliche del progetto.

## Compatibilità: cosa sappiamo e cosa no

- **Modello confermato:** INNO-Hit IH55UWB; firmware 04.56.35; il menu TV riporta webOS Hub 7.5.8.
- **Altro modello target:** IH55UWB4, non ancora confermato dall'utente e non testato.
- **Osservazione riportata dall'utente:** CSWeb TV 0.6.0 si avvia su IH55UWB, ma i plugin non funzionano. Non sono ancora noti variante IPK, plugin provato, stadio del guasto o log.
- **Verifica CI del profilo INNO-Hit:** il runtime ARMv7 ha caricato e registrato StreamingCommunity, AnimeUnity e AnimeWorld in test isolati/offline e il package 0.6.3 ha superato il controllo IPK; non equivale a una prova sulla TV.
- La variante ARMv7 è un profilo prudenziale, non una conclusione ricavata dal numero di versione. Kernel ABI, API webOS Hub, librerie grafiche/media e policy d'installazione vanno verificati sulla TV.

Dopo l'installazione raccogli log e risultati per **entrambe** le sigle, se possibile. Segui la matrice in [docs/roadmap.md](docs/roadmap.md) e la guida upstream [compatibilità](docs/compatibility.md).

## Riferimenti e licenze

- [CloudStream Android](https://github.com/recloudstream/cloudstream)
- [CSWeb TV upstream](https://github.com/sostk/cloudstream-webos-native)
- [Waydroid](https://github.com/waydroid/waydroid)
- [doGior's Had Enough](https://github.com/doGior/doGiorsHadEnough)

Il player nativo webOS accetta link HTTP(S) diretti e un sottoinsieme dei deep link player Stremio; torrent/magnet richiedono lo Stremio Streaming Server e non sono supportati. L'handoff dal client Stremio su webOS non è stato verificato; dettagli e limiti in [`docs/research/stremio-webos-player.md`](docs/research/stremio-webos-player.md).

Questo progetto non è affiliato a LG, INNO-Hit, reCloudStream, Waydroid, doGior, Stremio o agli autori di CSWeb TV. Il codice derivato è GPL-3.0-or-later, che **consente anche usi commerciali**: non è stata applicata una restrizione non commerciale perché upstream e kit sono già GPL e non è documentata l'autorizzazione a relicenziare quel codice. Le licenze dei componenti e dei plugin restano separate. Consulta [`LICENSING.md`](LICENSING.md), [`UPSTREAM.md`](UPSTREAM.md) e gli avvisi sotto `runtime/licenses*` prima di redistribuire una build.
