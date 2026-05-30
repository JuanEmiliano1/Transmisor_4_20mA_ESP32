% 1. Suavizado con filtro promedio móvil (usando la instrucción 'filter')
tamano_ventana = 10; % Promedia las últimas 10 muestras
b = (1/tamano_ventana) * ones(1, tamano_ventana);
a = 1;
voltaje_suavizado = filter(b, a, voltaje_acondicionado);

% 2. Remodelado de datos (usando la instrucción 'reshape')
% El manual pide usar reshape. Vamos a transformar nuestro vector de 1x200
% en una matriz de 20x10. Esto puede ser útil si quieres analizar estadísticas por bloques de tiempo.
matriz_bloques = reshape(voltaje_acondicionado, [20, 10]);

% 3. Envolvente (usando la instrucción 'envelope')
% Calcula las envolventes superior e inferior para ver la tendencia de los picos
[env_superior, env_inferior] = envelope(voltaje_acondicionado, 15, 'peak');

% --- Gráfica Comparativa de Resultados ---
figure('Name', 'Análisis de Tendencia', 'NumberTitle', 'off');
hold on;

% Señal original en gris claro
plot(voltaje_acondicionado, 'Color', [0.7 0.7 0.7], 'DisplayName', 'Señal Original Cruda');

% Señal suavizada (filtro) en azul
plot(voltaje_suavizado, 'b', 'LineWidth', 2, 'DisplayName', 'Promedio Móvil (Filtro)');

% Envolventes en rojo y verde
plot(env_superior, 'r--', 'LineWidth', 1.5, 'DisplayName', 'Envolvente Superior');
plot(env_inferior, 'g--', 'LineWidth', 1.5, 'DisplayName', 'Envolvente Inferior');

hold off;
legend('Location', 'best');
title('Análisis y Suavizado de la Señal del PT100');
xlabel('Número de Muestra');
ylabel('Voltaje (V)');
grid on;
