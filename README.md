# CloudStream webOS Hub — INNO-Hit 55 profile

Port nativo per webOS Hub, basato sul progetto community [CSWeb TV](https://github.com/sostk/cloudstream-webos-native). L'app usa UI e player nativi webOS e un runtime locale separato per i plugin CloudStream compatibili: **non installa APK Android e non avvia una sessione Waydroid**. Waydroid è usato come riferimento per separazione e packaging del runtime, non come container da incorporare nella TV. Dettagli in [architettura](docs/architecture.md) e [provenienza del codice](UPSTREAM.md).

## Profilo INNO-Hit 55″

Il profilo `CSWEB_INNOHIT_55` è per la famiglia di modelli pubblicata da INNO-Hit come **IH55UWB** e **IH55UWB4**. La scheda del produttore elenca per entrambi i codici la stessa piattaforma (CPU ARM Cortex-A55 quad-core, 1,5 GB RAM, 8 GB ROM, webOS 5.0). Il profilo crea **un singolo IPK** e seleziona la variante ARMv7 con `libcurl` incluso, così non dipende dalla `libcurl.so.4` del firmware più recente. Non è una prova di compatibilità: il pacchetto va ancora installato e testato su entrambe le varianti hardware.

Hai riportato firmware **04.56.35** e diagonale 55″. Se trovi il codice sull'etichetta, controlla che sia `IH55UWB` o `IH55UWB4`; non pubblicare il numero seriale. Un modello diverso potrebbe richiedere una build distinta.

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

- **Obiettivo software:** i due codici INNO-Hit 55″ indicati sopra, webOS Hub 5.0.
- **Dispositivo dell'utente:** firmware 04.56.35, TV da 55″; codice modello non ancora confermato.
- **Verifica su INNO-Hit:** non ancora eseguita. L'IPK che supera `check-package` non equivale a una prova sulla TV.
- La variante ARMv7 è scelta per il firmware webOS 5.x; il kernel ABI, l'API webOS Hub, le librerie grafiche/media e le policy d'installazione devono essere verificati sul dispositivo.

Dopo l'installazione raccogli log e risultati per **entrambe** le sigle, se possibile. Segui la matrice in [docs/roadmap.md](docs/roadmap.md) e la guida upstream [compatibilità](docs/compatibility.md).

## Riferimenti e licenze

- [CloudStream Android](https://github.com/recloudstream/cloudstream)
- [CSWeb TV upstream](https://github.com/sostk/cloudstream-webos-native)
- [Waydroid](https://github.com/waydroid/waydroid)
- [doGior's Had Enough](https://github.com/doGior/doGiorsHadEnough)

Questo progetto non è affiliato a LG, INNO-Hit, reCloudStream, Waydroid, doGior o agli autori di CSWeb TV. Il codice derivato è GPL-3.0-or-later; restano valide le licenze dei componenti e dei plugin di terze parti. Leggi [`UPSTREAM.md`](UPSTREAM.md) e gli avvisi sotto `runtime/licenses*` prima di redistribuire una build.
