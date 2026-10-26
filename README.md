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

Il target package è `org.csweb.tv.legacy`; il titolo visualizzato nell'app è **CloudStream Hub - INNO-Hit 55**. Il file IPK viene creato sotto `external/webos-native-app-kit/build/webos-innohit55/dist/`. La build può essere avviata anche da GitHub Actions sulla branch `arena/...`; l'artifact appare nella run CI quando i job ARMv7/webOS terminano. I job Legacy sono sperimentali e possono fallire senza bloccare i test host.

### Aggiungere doGior

Dopo l'installazione, apri **Settings → Extensions → Add repository** e inserisci il manifest ufficiale:

```text
https://raw.githubusercontent.com/doGior/doGiorsHadEnough/builds/repo.json
```

La repo è mantenuta da terzi, quindi non viene congelata nell'IPK: aggiungila nell'app per ottenere l'elenco aggiornato. La compatibilità va verificata per singolo plugin; WebView/UI Android, DRM, download e servizi non implementati possono non funzionare.

## Compatibilità: cosa sappiamo e cosa no

- **Modello confermato:** INNO-Hit IH55UWB; firmware 04.56.35; il menu TV riporta webOS Hub 7.5.8.
- **Altro modello target:** IH55UWB4, non ancora confermato dall'utente e non testato.
- **Verifica su INNO-Hit:** non ancora eseguita. L'IPK che supera `check-package` non equivale a una prova sulla TV.
- La variante ARMv7 è un profilo prudenziale, non una conclusione ricavata dal numero di versione. Kernel ABI, API webOS Hub, librerie grafiche/media e policy d'installazione vanno verificati sulla TV.

Dopo l'installazione raccogli log e risultati per **entrambe** le sigle, se possibile. Segui la matrice in [docs/roadmap.md](docs/roadmap.md) e la guida upstream [compatibilità](docs/compatibility.md).

## Riferimenti e licenze

- [CloudStream Android](https://github.com/recloudstream/cloudstream)
- [CSWeb TV upstream](https://github.com/sostk/cloudstream-webos-native)
- [Waydroid](https://github.com/waydroid/waydroid)
- [doGior's Had Enough](https://github.com/doGior/doGiorsHadEnough)

Questo progetto non è affiliato a LG, INNO-Hit, reCloudStream, Waydroid, doGior o agli autori di CSWeb TV. Il codice derivato è GPL-3.0-or-later; restano valide le licenze dei componenti e dei plugin di terze parti. Leggi [`UPSTREAM.md`](UPSTREAM.md) e gli avvisi sotto `runtime/licenses*` prima di redistribuire una build.
