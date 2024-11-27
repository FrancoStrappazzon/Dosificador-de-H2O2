# Sistema Dosificador de Peróxido de Hidrogeno
## Tesis de grado de Ingeniería Electrónica

Este proyecto integrador consiste en el diseño, desarrollo e implementación de un sistema dosificador de peróxido de hidrógeno (agua oxigenada). Utilizando un sistema de control mediante una cámara, se monitorea
la banda del espectro lumínico que corresponde a pigmentos específicos de las cianobacterias encontradas en el embalse San Roque. De esta forma, se puede estimar la cantidad de algas que se encuentran en ese lugar
en particular y así poder dosificar la cantidad adecuada de peróxido de hidrógeno para evitar la proliferación de las mismas.
El sistema consta de un módulo autónomo de cámara y un módulo actuador con el tanque y las válvulas. Estos módulos se comunican utilizando el protocolo ESP-NOW para el control de las válvulas. Ambos módulos utilizan comunicación WiFi para conectarse con un servidor web mediante el cual se puede configurar parámetros relevantes para cada módulo. Además, el módulo actuador utiliza comunicación WiFi para monitorear la cantidad de peróxido de hidrógeno que queda disponible en el tanque, de esta forma poder alertar mediante una API que se comunica con la aplicación WhatsApp a un usuario si es necesario una reposición de peróxido en el tanque.
De esta forma se propone implementar un sistema automático que mitigue los efectos negativos de la eutrofización en focos específicos del embalse.

## Disposición General
![image](https://github.com/user-attachments/assets/71fbda59-df22-43c7-8046-69d9e906c0cd)

## Prototipo
![image](https://github.com/user-attachments/assets/05ba65b6-89fa-4ccc-9d03-98fbc93922ab)

## Mensaje via whatsapp
![image](https://github.com/user-attachments/assets/abdbe317-66ca-4cbb-b17c-9c305babe0a2)

## Páginas para configuración de módulos
![image](https://github.com/user-attachments/assets/07c7d9a2-e999-4b5c-aaec-8d5986efe3b0)

## Resultados obtenidos
![image](https://github.com/user-attachments/assets/ef9fed2d-c0ff-417b-ba7c-1f2d16272a9d)


![image](https://github.com/user-attachments/assets/37d734b6-7f12-482e-9b88-57aecf5bfd51)
