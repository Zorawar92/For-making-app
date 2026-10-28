# Architettura: port nativo webOS, con Android runtime ristretto

## Decisione

Costruire una **nuova app webOS**, non un contenitore grafico Android. Waydroid è utile per studiare come assemblare ed eseguire un ambiente Android su Linux, ma non è un formato di portabilità per webOS e non produce IPK installabili su webOS.

Waydroid dichiara di avviare un sistema Android completo in un container e dipende da Linux namespaces (user, pid, uts, net, mount, ipc), da un'immagine Android/LineageOS e dall'accesso alle risorse hardware. Il target dichiarato è una TV INNO-Hit con **webOS Hub**, non hardware LG: kernel, SoC, firmware e policy possono differire sia tra modelli OEM sia dal webOS TV di riferimento. Non si deve assumere la compatibilità degli IPK per LG, né la disponibilità di LXC, namespace, driver Binder o privilegi necessari ad avviare quel container. Inserire l'intera immagine Waydroid nell'IPK sarebbe inoltre molto pesante e non renderebbe automaticamente nativa l'interazione con telecomando, video e storage.

## Decisione dopo la valutazione di Android Translation Layer (ATL)

L'ATL indicato dall'utente (`android_translation_layer/android_translation_layer`) è un layer per eseguire APK Android su Linux desktop: avvia ART e fornisce implementazioni Android sopra GTK/Wayland, con JNI/native shims. Non è un runtime webOS già impacchettato e non produce da solo un IPK compatibile con webOS Hub. La revisione consultata del mirror GitHub `external-mirrors/android_translation_layer` è `b17fe9a`.

Il port webOS che ho trovato, [WebOS-ATL](https://github.com/sarahahzub/WebOS-ATL), è un progetto distinto basato su ATL. Il suo README dichiara requisiti di TV LG rooted, Homebrew Channel con root OK, CPU `aarch64` e test solo su LG 43UR7500/webOS 23; inoltre esegue gli APK come root fuori dal sandbox webOS e usa hook di avvio, servizi privilegiati e interfacce specifiche LG. Il proprietario ha confermato che l'IH55UWB non è rooted. Perciò **non integreremo né installeremo WebOS-ATL sull'IH55UWB**: i suoi prerequisiti non sono soddisfatti e l'esecuzione root sarebbe una superficie di rischio non giustificata.

La scelta per questo dispositivo è mantenere l'applicazione **nativa webOS** già basata su CSWeb TV. L'ATL resta un riferimento tecnico per ART e per valutare singole API Android; non verranno copiati in blocco GTK/Wayland, boot hook o servizi root. L'app gira sulla IH55UWB secondo il test dell'utente; ora si affrontano, separatamente, la somiglianza grafica con CloudStream Android e la compatibilità dei provider.

## Cosa riutilizzare dai progetti di riferimento

### CSWeb TV

CSWeb TV è la prova di fattibilità e il riferimento implementativo principale. Il suo modello architetturale è già ibrido, non una conversione APK:

1. app e interfaccia televisiva native;
2. runtime ART avviato come processo separato per il bytecode dei plugin `.cs3`;
3. API/compatibilità Android limitata alle chiamate che i provider supportati usano;
4. scambio richieste/risultati attraverso un protocollo locale;
5. playback attraverso la pipeline media nativa webOS.

Per un nuovo port useremo questa architettura come benchmark, riutilizzando codice solo se serve e rispettando licenze, avvisi e attribuzione. Prima di reinventare ART, plugin loading e player è necessario confrontare lo scope con la release e il codice aggiornati di CSWeb TV.

### Waydroid

Dallo studio di Waydroid si possono prendere idee relative a:

- processo/runtime separato dal frontend;
- root filesystem e dipendenze versionati;
- controllo esplicito di architettura, ABI e compatibilità kernel;
- diagnostica e test riproducibili dell'ambiente Android.

Non si assume invece di poter portare su webOS l'intero container Waydroid, il suo system image o le sue dipendenze Linux. Se un test sul dispositivo dimostrasse che una parte è indispensabile, va valutata separatamente e non deve diventare una dipendenza silenziosa dell'IPK.

## Componenti previsti

```text
IPK webOS
├── UI nativa TV (telecomando, catalogo, ricerca, dettagli)
├── gestore repository/plugin (.cs3) con import di bundle offline opzionale
├── client IPC locale con timeout, limiti messaggi e log senza token
├── player webOS e adattatore link/sottotitoli
└── runtime helper locale
    ├── ART/compatibilità Android minima, solo API necessarie ai provider supportati
    ├── API CloudStream/JVM bloccate a revisioni e hash noti
    └── protocollo strutturato per ricerca, dettagli, episodi e stream
```

Il browser/HTML5 può essere usato solo se necessario per un elemento UI isolato; non è l'obiettivo di esecuzione per il plugin Android. Il percorso primario è app webOS nativa con toolchain e packaging webOS.

## Confini di sicurezza e compatibilità

I plugin sono codice di terze parti con accesso di rete. Il runtime helper dovrebbe essere un processo distinto per robustezza, ma questo **non equivale** a una sandbox di sicurezza: su webOS i processi dell'app possono condividere lo stesso utente e privilegi. Non distribuire credenziali o segreti nel pacchetto. Limitare dimensione dei plugin, verificare hash dove disponibile, evitare log di URL con token e rendere visibili all'utente le fonti aggiunte.

La compatibilità va dichiarata per combinazione di:

- modello TV, versione webOS/firmware, architettura kernel e memoria disponibile;
- ABI del runtime e librerie effettivamente presenti nel firmware;
- API Android/CloudStream richieste dal singolo plugin;
- protocolli, codec, DRM, header, TLS e limiti del player nativo.

Non promettere compatibilità generale con tutti i provider Cloudstream. In particolare, WebView, UI Android/AndroidX, DRM Widevine/PlayReady, download e plugin che dipendono da servizi Google richiedono supporto dedicato o possono rimanere esclusi.

## Fonte doGior

Il manifest remoto da offrire nella UI (senza congelare l'elenco dei plugin nell'IPK) è:

```text
https://raw.githubusercontent.com/doGior/doGiorsHadEnough/refs/heads/builds/repo.json
```

Il programma di build locale può, con opt-in esplicito, scaricare StreamingCommunity, AnimeUnity e AnimeWorld dalla revisione fissata del ramo `builds`, verificare SHA-256 e formato `.cs3`, poi includerli nell'IPK personale. Al primo avvio il manager importa e abilita i plugin mancanti dallo storage locale e registra la repo; una copia già installata dall'utente non viene sovrascritta. I binari restano fuori da Git e dalle Release pubbliche: la repo non dichiara una licenza e l'inclusione pubblica richiede verifica/permesso. L'import offline non implica compatibilità delle API o del playback sulla IH55UWB.

## Riferimenti primari

- [Waydroid README](https://github.com/waydroid/waydroid): container Android completo e requisiti Linux.
- [CSWeb TV README](https://github.com/sostk/cloudstream-webos-native): architettura, pacchetti, limiti e guida di build.
- [Decisione di fattibilità CSWeb TV](https://github.com/sostk/cloudstream-webos-native/blob/main/docs/FEASIBILITY_DECISION.md): esempi di plugin provati, limiti e risorse misurate.
- [doGior manifest](https://raw.githubusercontent.com/doGior/doGiorsHadEnough/refs/heads/builds/repo.json).
