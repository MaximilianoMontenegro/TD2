# Análisis en Python del Monitor de ECG (TD II – UTN FRBA)

Scripts que generan las gráficas de ECG, filtros y validación del informe
(`Informe_Monitor_ECG_TD2.docx`) a partir del registro real grabado por el
equipo en la microSD. Sirven también para analizar registros nuevos y para
probar cambios en los filtros o en el detector **antes** de grabarlos en la placa.

## 1. Instalación (una sola vez)

Hace falta Python 3.10 o más nuevo (https://www.python.org). En una consola,
dentro de esta carpeta:

```
pip install -r requirements.txt
```

Eso instala numpy, scipy, matplotlib y `ziglang` (un compilador de C que se
instala con pip; solo se usa para probar en la PC el código C del firmware).

## 2. Uso rápido

```
python generar_todo.py
```

Genera todas las figuras en `figuras/` y la tabla de validación en
`resultados/validacion.txt` (tarda alrededor de un minuto).

## 3. Después de cada prueba con la placa

Copiar de la microSD el archivo de la sesión (`ECG_0001.CSV`, `ECG_0002.CSV`, …;
el número es `sd_file_num` en Live Expressions) y correr:

```
python analizar_registro.py D:\ECG_0001.CSV --mostrar
python analizar_registro.py D:\ECG_0001.CSV --desde 30 --hasta 40
```

Informa qué porcentaje del tiempo hubo LEAD OFF, ruido y señal limpia, y la
mediana, el mínimo y el máximo de FC, R-R, QRS y onda R con señal limpia.
Además vuelve a calcular la FC en la PC con el mismo detector y la compara
con la que guardó la placa: más de 90 % de coincidencia indica que el
firmware funciona igual que el modelo. Deja la figura en
`figuras/sesion_ECG_0001.png` y el resumen en `resultados/sesion_ECG_0001.txt`.

## 4. Qué hace cada archivo

| Archivo | Qué hace | Figuras del informe |
|---|---|---|
| `analizar_registro.py` | Resumen y comparación placa vs. PC de una sesión nueva de la SD | — |
| `ecg_comun.py` | Coeficientes de los filtros (iguales al firmware), lectura del CSV, ECG sintético, tramos del registro | — (lo usan los demás) |
| `detector_ref.py` | Detector de QRS + SQI en Python (copia de `ecg_qrs.c`) y el detector anterior | — |
| `graficos_filtros.py` | Respuesta en frecuencia de los filtros, coeficientes del FIR y efecto del filtrado | 8, 9, 10 |
| `graficos_ecg.py` | ECG real con sus ondas, etapas de Pan-Tompkins, plantilla del SQI, registro completo antes/después | 1, 11, 13, 14 |
| `correr_detector_c.py` | Compila y ejecuta en la PC el **mismo** `ecg_qrs.c` del firmware | — |
| `validacion.py` | Tabla de resultados por tramo (Tabla 7 del informe) y casos sintéticos | Tabla 7 |
| `generar_todo.py` | Corre todo lo anterior en orden | todas |
| `validacion_c/` | `harness.c` (banco de prueba) y copias de `ecg_qrs.c/.h` y `ecg_config.h` | — |
| `datos/ECG_LOG.CSV` | Registro real de 249 s grabado por el equipo | — |

Cada script tiene al principio una explicación detallada y acepta `--help`:

```
python graficos_ecg.py --help
```

## 5. Ejemplos

Ver las figuras en pantalla (además de guardarlas):

```
python graficos_filtros.py --mostrar
python graficos_ecg.py --mostrar
```

Analizar un registro nuevo de la SD (los archivos `ECG_0001.CSV`, `ECG_0002.CSV`, ...):

```
python graficos_ecg.py --csv D:/ECG_0003.CSV --limpio 30 45 --ruido 60 70
python correr_detector_c.py --csv D:/ECG_0003.CSV
```

`--limpio` es un tramo (en segundos) donde la señal se ve bien y `--ruido`
uno donde no; se eligen mirando la columna `ecg` del CSV en Excel o con
`--mostrar`. El CSV nuevo trae además la columna `sqi` (1 = el equipo
consideró la señal confiable).

Probar otro filtro: cambiar en `ecg_comun.py`, por ejemplo,
`FIR_TAPS = 83` por `FIR_TAPS = 61`, o la frecuencia de corte de `firwin`, y
volver a correr `python graficos_filtros.py`.

Probar otro parámetro del detector: cambiar las constantes al principio de
`detector_ref.py` (por ejemplo `CC_MIN = 0.66`) y correr `python validacion.py`
para ver cómo cambia la tabla. Si el cambio sirve, hay que hacer el mismo
cambio en `Core/Src/ecg_qrs.c` y verificar con `correr_detector_c.py`.

## 6. Notas

- La columna `adc` (formato viejo) o `ecg` (formato nuevo) del CSV es el ECG
  **ya filtrado** por el equipo, en counts del ADC (1 mV ≈ 1365 counts).
- Si esta carpeta está dentro del proyecto (`docs/analisis_python`),
  `correr_detector_c.py` toma automáticamente la versión actual de
  `Core/Src/ecg_qrs.c`. Si se copia a otra PC sin el proyecto, usa las
  copias de `validacion_c/`.
- La clasificación de tramos (`TRAMOS` en `ecg_comun.py`) corresponde solo al
  registro `datos/ECG_LOG.CSV`; se hizo mirando la señal y el video de la prueba.
- Resultado esperado de la validación: el detector anterior muestra una FC
  errónea el 78,6 % del tiempo en los tramos sin ECG; el actual, 0 %. La
  referencia en Python y el código C coinciden en el 100 % de las muestras.
