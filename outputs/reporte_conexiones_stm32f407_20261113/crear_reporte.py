from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.shared import Cm, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
import math

ROOT=Path(__file__).parent
QA=ROOT/'qa'
BLUE='#235F88'; INK='#192E3D'; FILL='#EDF4F8'; GRAY='#596774'
FONT='C:/Windows/Fonts/arial.ttf'; BOLD='C:/Windows/Fonts/arialbd.ttf'

def canvas(w,h):
    im=Image.new('RGB',(w,h),'white'); return im,ImageDraw.Draw(im)
def txt(d,xy,s,size=30,bold=False,color=INK,anchor='mm'):
    d.text(xy,s,font=ImageFont.truetype(BOLD if bold else FONT,size),fill=color,anchor=anchor)
def box(d,xy,title,lines,size=30):
    d.rounded_rectangle(xy,radius=13,fill=FILL,outline=BLUE,width=3)
    x1,y1,x2,y2=xy; x=(x1+x2)/2
    total=44+len(lines)*40; y=(y1+y2-total)/2+20
    txt(d,(x,y),title,size+3,True)
    for i,line in enumerate(lines): txt(d,(x,y+52+i*40),line,size)
def arrow(d,points,color=BLUE,dashed=False,both=False):
    if dashed:
        for a,b in zip(points,points[1:]):
            length=math.dist(a,b)
            for start in range(0,int(length),28):
                end=min(start+15,length)
                d.line([(a[0]+(b[0]-a[0])*start/length,a[1]+(b[1]-a[1])*start/length),(a[0]+(b[0]-a[0])*end/length,a[1]+(b[1]-a[1])*end/length)],fill=color,width=3)
    else: d.line(points,fill=color,width=3)
    def head(a,b):
        t=math.atan2(b[1]-a[1],b[0]-a[0]); L=17
        d.polygon([b,(b[0]-L*math.cos(t-.45),b[1]-L*math.sin(t-.45)),(b[0]-L*math.cos(t+.45),b[1]-L*math.sin(t+.45))],fill=color)
    head(points[-2],points[-1])
    if both: head(points[1],points[0])

im,d=canvas(1500,970)
box(d,(20,30,410,215),'BMI160',['Aceleración y giro','I²C1 · 0x68 propuesto'])
box(d,(20,285,410,470),'BMP388',['Presión y temperatura','SPI1 · CS por GPIO'])
box(d,(20,540,410,725),'ASPD 4525',['Presión diferencial','I²C2 independiente'])
box(d,(570,135,1470,625),'STM32F407VET6',['Tarjeta STM32F4XX M','Adquirir y fechar mediciones','Calibrar y filtrar','Estimar variables y su validez','Preparar estado para la computadora'],size=32)
for y in [122,377,632]: arrow(d,[(410,y),(490,y),(490,380),(570,380)])
box(d,(650,740,1000,930),'CP2102N',['UART a USB','Lógica de 3,3 V'])
box(d,(1120,740,1480,930),'Computadora',['Validar mensajes','Visualizar six pack'])
arrow(d,[(1015,625),(1015,680),(825,680),(825,740)])
txt(d,(1200,677),'UART 115200 · 8N1',29)
arrow(d,[(1000,835),(1120,835)]); txt(d,(1060,800),'USB',27)
box(d,(20,800,410,950),'Entrada manual SIL',['Estado simulado'],size=29)
arrow(d,[(410,877),(540,877),(540,960),(1300,960),(1300,930)],dashed=True)
im.save(QA/'01_funcional.png')

im,d=canvas(1500,1130)
d.rounded_rectangle((475,20,1030,1090),radius=13,fill=FILL,outline=BLUE,width=3)
txt(d,(752,65),'STM32F407VET6',35,True)
# Central labels are grouped by peripheral to make the port names legible.
for y,title,lines in [
 (180,'I²C1  AF4',['PB6  SCL     PB7  SDA','PC4  EXTI4']),
 (475,'SPI1  AF5',['PA5 SCK  PA7 MOSI','PA6 MISO  PA4 CS','PB0 EXTI0']),
 (775,'USART1  AF7',['PA9 TX    PA10 RX']),
 (970,'Depuración SWD',['PA13 SWDIO  PA14 SWCLK'])]:
    txt(d,(752,y),title,32,True)
    for j,s in enumerate(lines): txt(d,(752,y+48+43*j),s,30)
box(d,(15,90,405,330),'BMI160',['SCL ↔ PB6','SDA ↔ PB7','INT1 → PC4','CSB a 3,3 V · SDO a GND'],size=28)
box(d,(15,400,405,680),'CJMCU 388',['SCK ← PA5','SDI ← PA7 · SDO → PA6','CS ← PA4','INT → PB0'],size=29)
box(d,(15,900,405,1090),'ST LINK',['SWDIO ↔ PA13','SWCLK → PA14','NRST opcional · GND'],size=28)
box(d,(1100,90,1485,345),'ASPD 4525',['SCL ↔ PB10','SDA ↔ PB11','I²C2 AF4','Niveles según módulo'],size=28)
box(d,(1100,680,1485,910),'CP2102N',['RXD ← PA9','TXD → PA10','GND común','USB a computadora'],size=28)
arrow(d,[(475,160),(405,160)])
arrow(d,[(405,210),(475,210)],both=True)
arrow(d,[(405,260),(475,260)])
arrow(d,[(475,455),(405,455)])
arrow(d,[(405,505),(475,505)],both=True)
arrow(d,[(475,555),(405,555)])
arrow(d,[(405,605),(475,605)])
arrow(d,[(405,965),(475,965)],both=True)
arrow(d,[(405,1015),(475,1015)])
arrow(d,[(1030,210),(1100,210)],both=True)
txt(d,(1070,410),'I²C2',26); txt(d,(1255,453),'PB10 SCL · PB11 SDA',28)
arrow(d,[(1030,760),(1100,760)]); arrow(d,[(1100,815),(1030,815)])
im.save(QA/'02_conexiones.png')

im,d=canvas(1500,930)
box(d,(480,20,1020,160),'Fuente regulada de 5 V',['Una fuente para tarjeta y ASPD'])
box(d,(35,290,655,510),'Tarjeta STM32F4XX M',['Entrada 5 V de la tarjeta','Regulador interno a 3,3 V','MCU y GPIO a 3,3 V'])
box(d,(1020,290,1480,510),'ASPD 4525',['Entrada 5 V','Rango del módulo 4 a 5,5 V','Verificar niveles de I²C2'],size=28)
arrow(d,[(750,160),(750,230),(345,230),(345,290)])
arrow(d,[(750,230),(1250,230),(1250,290)])
txt(d,(400,257),'5 V',30,True); txt(d,(1300,257),'5 V',30,True)
box(d,(200,650,700,830),'Módulo BMI160',['Alimentación efectiva 3,3 V','Lógica I²C e INT a 3,3 V'],size=28)
box(d,(820,650,1370,830),'Módulo CJMCU 388',['VIN a 3,3 V condicionado al módulo','3Vo sin conectar'],size=28)
arrow(d,[(345,510),(345,570),(450,570),(450,650)])
arrow(d,[(345,570),(1095,570),(1095,650)])
txt(d,(755,540),'3,3 V de la tarjeta · verificar capacidad de corriente',29)
d.line([(20,890),(1480,890)],fill=GRAY,width=4)
for x,y in [(100,510),(1420,510),(450,830),(1095,830)]: d.line([(x,y),(x,890)],fill=GRAY,width=3)
txt(d,(750,920),'GND común con CP2102N y ST LINK',30,True)
im.save(QA/'03_alimentacion.png')

doc=Document(); sec=doc.sections[0]
sec.page_width=Cm(21); sec.page_height=Cm(29.7)
sec.top_margin=Cm(1.5); sec.bottom_margin=Cm(1.5); sec.left_margin=Cm(1.7); sec.right_margin=Cm(1.7)
for name in ['Normal','Title','Subtitle','Heading 1','Heading 2','Caption']:
    s=doc.styles[name]; s.font.name='Arial'; s.font.color.rgb=RGBColor(0,0,0)
    s.paragraph_format.space_after=Pt(6)
    pp=s.element.get_or_add_pPr()
    border=OxmlElement('w:pBdr')
    for side in ['top','left','bottom','right','between']:
        edge=OxmlElement('w:'+side);edge.set(qn('w:val'),'nil');border.append(edge)
    pp.append(border)
doc.styles['Normal'].font.size=Pt(10.5)
doc.styles['Normal'].paragraph_format.line_spacing=1.08
doc.styles['Title'].font.size=Pt(22)
doc.styles['Heading 1'].font.size=Pt(15)
doc.styles['Heading 2'].font.size=Pt(11.5)
doc.styles['Caption'].font.size=Pt(9)
doc.core_properties.title='Diagramas y conexiones del SixPack con STM32F407VET6'
doc.core_properties.subject='STM32F4XX M BMI160 BMP388 ASPD 4525'
doc.core_properties.author=''

def p(s,style=None): return doc.add_paragraph(s,style)
def h(s): doc.add_heading(s,1)
def small(s):
    x=p(s)
    for r in x.runs:r.font.size=Pt(9)
    return x
def fig(name,caption,width=17.5):
    q=doc.add_paragraph(); q.alignment=WD_ALIGN_PARAGRAPH.CENTER
    q.add_run().add_picture(str(QA/name),width=Cm(width))
    q.paragraph_format.space_after=Pt(3)
    cap=p(caption,'Caption'); cap.paragraph_format.keep_with_next=True
    inline=q.runs[0]._r.xpath('.//wp:docPr')[0]; inline.set('descr',caption)
def table(headers,rows,widths,size=9.5):
    t=doc.add_table(rows=1,cols=len(headers)); t.alignment=WD_TABLE_ALIGNMENT.CENTER; t.autofit=False
    for cell,w in zip(t.columns,widths):cell.width=Cm(w)
    for i,(vals,ishead) in enumerate([(headers,True)]+[(r,False) for r in rows]):
        cells=t.rows[0].cells if ishead else t.add_row().cells
        trPr=cells[0]._tc.getparent().get_or_add_trPr()
        no=OxmlElement('w:cantSplit');trPr.append(no)
        if ishead: rep=OxmlElement('w:tblHeader');trPr.append(rep)
        for j,(c,value,w) in enumerate(zip(cells,vals,widths)):
            c.width=Cm(w);c.vertical_alignment=WD_CELL_VERTICAL_ALIGNMENT.CENTER
            pr=c._tc.get_or_add_tcPr()
            sh=OxmlElement('w:shd');sh.set(qn('w:fill'),'235F88' if ishead else ('EDF4F8' if i%2 else 'FFFFFF'));pr.append(sh)
            margins=OxmlElement('w:tcMar')
            for side in ['top','bottom','left','right']:
                el=OxmlElement('w:'+side);el.set(qn('w:w'),'75');el.set(qn('w:type'),'dxa');margins.append(el)
            pr.append(margins)
            borders=OxmlElement('w:tcBorders')
            for side in ['top','left','bottom','right']:
                el=OxmlElement('w:'+side);el.set(qn('w:val'),'single');el.set(qn('w:sz'),'4');el.set(qn('w:color'),'D9D9D9');borders.append(el)
            pr.append(borders)
            q=c.paragraphs[0];q.paragraph_format.space_after=Pt(0);q.paragraph_format.line_spacing=1.0
            q.alignment=WD_ALIGN_PARAGRAPH.CENTER if j==0 else WD_ALIGN_PARAGRAPH.LEFT
            r=q.add_run(value);r.font.size=Pt(size);r.bold=ishead
            if ishead:r.font.color.rgb=RGBColor(255,255,255)
    doc.add_paragraph().paragraph_format.space_after=Pt(0)
    return t
def page(): doc.add_page_break()
def link(label,url):
    q=doc.add_paragraph();q.paragraph_format.space_after=Pt(3)
    hl=OxmlElement('w:hyperlink');hl.set(qn('r:id'),q.part.relate_to(url,'http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink',is_external=True))
    r=OxmlElement('w:r');rp=OxmlElement('w:rPr');sz=OxmlElement('w:sz');sz.set(qn('w:val'),'18');rp.append(sz);r.append(rp)
    te=OxmlElement('w:t');te.text=label;r.append(te);hl.append(r);q._p.append(hl)

p('Diagramas y conexiones del SixPack con STM32F407VET6','Title')
p('Tarjeta STM32F4XX M con BMI160 y BMP388','Subtitle')
small('Propuesta de ingeniería de banco • 13 de noviembre de 2026')
p('La actualización sustituye el STM32F103C8T6, el MPU6050 y el BMP180 por el STM32F407VET6, el BMI160 y el BMP388. Se conserva la arquitectura con ASPD-4525, puente CP2102N y depuración ST-LINK. Las asignaciones siguientes son propuestas de cableado pendientes de comprobación física e integración con el firmware.')
h('Diagrama funcional')
fig('01_funcional.png','Figura 1. Adquisición de sensores y envío del estado al six pack de la computadora.')
p('El BMI160 entrega aceleración y velocidad angular; el BMP388 mide presión absoluta y temperatura; el ASPD-4525 mide presión diferencial. La STM32 adquiere las lecturas y prepara las variables que se transmitirán por UART. El CP2102N convierte el enlace en un puerto USB para la computadora. [1–4]')
p('La entrada SIL alimenta la interfaz con un estado simulado. Se propone conservar UART a 115200 bit/s, ocho bits de datos, sin paridad y un bit de parada; el periodo de envío y el formato de mensajes deberán acordarse con el firmware y la aplicación.')
small('El BMI160 no incorpora magnetómetro. La arquitectura de sensores mostrada no aporta por sí sola una referencia absoluta de rumbo magnético.')

page(); h('Diagrama de conexiones de señal')
fig('02_conexiones.png','Figura 2. Asignación propuesta por nombre de GPIO y periférico. La alimentación se detalla en la figura 3.',17.4)
p('Los nombres PA y PB identifican GPIO, no posiciones físicas del conector. I²C1 e I²C2 usan AF4; SPI1 usa AF5; USART1 usa AF7; SWD conserva PA13 y PA14. PC4 y PB0 se usan como entradas GPIO con EXTI4 y EXTI0. [1]')
table(['Interfaz','Asignación propuesta','Condición inicial'],[
 ['BMI160','PB6 SCL · PB7 SDA · PC4 INT1','I²C1 a 100 kHz; 0x68 con SDO a GND'],
 ['BMP388','PA5 SCK · PA7 MOSI · PA6 MISO\nPA4 CS · PB0 INT','SPI1 de 4 hilos; modo 0; objetivo de reloj 1 MHz'],
 ['ASPD-4525','PB10 SCL · PB11 SDA','I²C2 a 100 kHz; niveles a comprobar'],
 ['PC y depuración','PA9 TX · PA10 RX\nPA13 SWDIO · PA14 SWCLK','UART a 3,3 V; SWD reservado']
],[2.7,7.3,7.6],9.3)
small('La frecuencia SPI depende del reloj APB2 y del prescaler disponible; 1 MHz es un objetivo inicial, no una medición. La velocidad del bus no determina la tasa de conversión del BMP388.')

page(); h('Conexiones de los sensores nuevos')
doc.add_heading('BMI160 mediante I²C1',2)
table(['Pin del módulo','Destino','Función y condición'],[
 ['VCC o VIN','3,3 V compatibles con el módulo','Confirmar la entrada y la regulación de la placa BMI160.'],
 ['GND','GND común','Referencia de alimentación y señales.'],
 ['SCL o SCx','PB6 · I²C1 SCL','Reloj; resistencia de elevación a 3,3 V.'],
 ['SDA o SDx','PB7 · I²C1 SDA','Datos bidireccionales; elevación a 3,3 V.'],
 ['CS o CSB','VDDIO a 3,3 V','Mantener alto para seleccionar I²C.'],
 ['SDO o SA0','GND','Selecciona dirección de siete bits 0x68.'],
 ['INT1','PC4 · EXTI4','Dato disponible propuesto; configurar la interrupción.'],
 ['INT2','Sin conectar','Reservado para una ampliación.']
],[3.4,4.6,9.6])
p('La interfaz primaria del BMI160 se configura en I²C con CSB alto y SDO a GND. [2] Se propone PC4 para INT1 porque el esquema de referencia conecta PA0 al botón de usuario. Revisar las resistencias ya presentes en el módulo antes de añadir otras. [6]')
doc.add_heading('CJMCU 388 mediante SPI1',2)
table(['Pin del CJMCU 388','Destino','Función y condición'],[
 ['VIN','3,3 V condicionados al módulo','Confirmar que la regulación mantiene VDD y VDDIO en rango.'],
 ['GND','GND común','Referencia de alimentación y señales.'],
 ['SCK','PA5 · SPI1 SCK','Reloj del maestro al sensor.'],
 ['SDI','PA7 · SPI1 MOSI','Datos de la STM32 hacia el BMP388.'],
 ['SDO','PA6 · SPI1 MISO','Datos del BMP388 hacia la STM32.'],
 ['CS','PA4 · salida GPIO','Selección activa en bajo; reposo en alto.'],
 ['INT','PB0 · EXTI0','Dato disponible; configurar su modo y polaridad.'],
 ['3Vo','Sin conectar','Función y conexión interna pendientes de confirmar.']
],[3.4,4.6,9.6])
p('Se propone SPI de cuatro hilos en modo 0. Las lecturas del BMP388 requieren un byte ficticio después de la dirección. [3] PA4 se controla como GPIO, con selección de esclavo por software. El rango del chip no confirma el rango VIN de la placa CJMCU.')

page(); h('Conexiones de comunicación y depuración')
doc.add_heading('ASPD 4525 mediante I²C2',2)
table(['Pin del módulo','Destino','Condición'],[
 ['5V','Fuente regulada de 5 V','Módulo especificado para 4 a 5,5 V. [4]'],
 ['GND','GND común','Compartido con la tarjeta STM32.'],
 ['SCL','PB10 · I²C2 SCL','Conexión mediante adaptación bidireccional si los niveles lo requieren.'],
 ['SDA','PB11 · I²C2 SDA','Conexión mediante adaptación bidireccional si los niveles lo requieren.']
],[3.2,5,9.4])
p('La separación de I²C2 permite tratar los niveles del ASPD sin exponer el BMI160. Antes de conectar, comprobar a qué tensión elevan SCL y SDA las resistencias del módulo y la compatibilidad de ambos extremos. Si los niveles no son compatibles, interponer un adaptador bidireccional I²C. La dirección 0x28 de la versión anterior queda como referencia por verificar en el módulo real.')
doc.add_heading('CP2102N y ST LINK',2)
table(['Señal externa','Destino STM32','Uso'],[
 ['RXD del CP2102N','PA9 · USART1 TX','Salida de datos de la STM32 hacia el puente.'],
 ['TXD del CP2102N','PA10 · USART1 RX','Entrada de datos o comandos hacia la STM32.'],
 ['GND del CP2102N','GND común','Referencia UART; puente alimentado por su USB.'],
 ['SWDIO del ST-LINK','PA13 · SWDIO','Datos de depuración bidireccionales.'],
 ['SWCLK del ST-LINK','PA14 · SWCLK','Reloj desde el depurador.'],
 ['GND del ST-LINK','GND común','Referencia de depuración.'],
 ['NRST del ST-LINK','NRST o RST','Conexión opcional de reinicio si está disponible.'],
 ['VTref del depurador','3,3 V de la tarjeta','Solo entrada de referencia si el modelo dispone de ella.']
],[4.5,4.5,8.6])
p('TX y RX se cruzan. El puente debe operar con lógica UART de 3,3 V; la tensión USB no define la tensión de sus señales. [5] Dejar sin conectar los pines de alimentación de salida del CP2102N y del ST-LINK cuando la tarjeta usa la fuente de banco. VTref no equivale a una salida de alimentación.')
small('En el esquema de la STM32F4XX M, la memoria W25Q16 usa PA15 como CS y PB3/PB4/PB5 para SPI1. Esta propuesta usa la ruta PA5/PA6/PA7 para el BMP388. Mantener la memoria sin seleccionar y no habilitar otra ruta del mismo periférico simultáneamente. [6]')

page(); h('Diagrama de alimentación')
fig('03_alimentacion.png','Figura 3. Distribución propuesta para banco con una fuente de 5 V y masa común.',17.5)
table(['Equipo','Alimentación propuesta','Condición de conexión'],[
 ['STM32F4XX M','5 V a la entrada de la tarjeta','Regulación interna a 3,3 V; confirmar revisión y conector.'],
 ['BMI160','3,3 V efectivos','Confirmar regulación de su módulo y lógica a 3,3 V.'],
 ['CJMCU-388','VIN a 3,3 V si es compatible','Verificar salida regulada; dejar 3Vo libre.'],
 ['ASPD-4525','5 V regulados','GND común; alimentación separada de los sensores de 3,3 V.'],
 ['CP2102N y ST-LINK','Su conexión USB','GND y señales a la tarjeta; evitar unir salidas de alimentación.']
],[3.6,5.7,8.3],9.3)
p('Verificar el consumo total y la capacidad térmica del regulador de la tarjeta antes de alimentar los sensores desde 3,3 V. El esquema de referencia muestra un AMS1117-3.3, pero su capacidad útil depende del montaje. [6] Si se conecta además el USB de la STM32, confirmar previamente el aislamiento entre las fuentes de 5 V.')
doc.add_heading('Referencias de los diagramas y tablas',2)
link('[1] STMicroelectronics · STM32F405xx y STM32F407xx · DS8626 · funciones alternativas', 'https://www.st.com/resource/en/datasheet/stm32f405rg.pdf')
link('[2] Bosch Sensortec · BMI160 · BST-BMI160-DS000-09 · interfaz I²C', 'https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi160-ds000.pdf')
link('[3] Bosch Sensortec · BMP388 · BST-BMP388-DS001-07 · interfaz SPI', 'https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp388-ds001.pdf')
link('[4] MATEKSYS · ASPD-4525 · especificaciones y cableado', 'https://www.mateksys.com/?portfolio=aspd-4525')
link('[5] Silicon Labs · CP2102N · alimentación y niveles VIO', 'https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf')
small('[6] Referencia local en Zenith/hardware/referencias/ZenithV3: original-schematic-STM32F407VGT6_STM32F4XX_M.pdf, hoja única del 23 de noviembre de 2018, y fotografía STM32F407VET6_STM32F4XX_M-1.jpg. Su correspondencia exacta con la revisión física debe confirmarse.')

(ROOT/'final').mkdir(exist_ok=True)
out=ROOT/'final'/'Reporte_diagramas_conexiones_STM32F407VET6_BMI160_BMP388.docx'
if out.exists(): raise FileExistsError(out)
doc.save(out)
print(out)
