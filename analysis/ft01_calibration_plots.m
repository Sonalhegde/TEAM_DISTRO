%% FT-01 Flow Sensor Calibration Plotter
% Model: Q = a * f + b
% Inputs: analysis/data/calibration_trials.csv
% Outputs: media/sensor_calibration_curve.png

clear; clc; close all;

script_dir = fileparts(mfilename('fullpath'));
data_file = fullfile(script_dir, 'data', 'calibration_trials.csv');

if ~exist(data_file, 'file')
    error('Data file not found: %s', data_file);
end

data = readtable(data_file);
f = data.freq_hz;
Q = data.actual_flow_Lpm;

% Linear regression Q = a*f + b
p = polyfit(f, Q, 1);
a = p(1);
b = p(2);

Q_pred = polyval(p, f);
residuals = Q - Q_pred;
ss_res = sum(residuals.^2);
ss_tot = sum((Q - mean(Q)).^2);
r_squared = 1 - (ss_res / ss_tot);
max_err = max(abs(residuals));
avg_err = mean(abs(residuals));

fprintf('=== FT-01 Calibration Results ===\n');
fprintf('Slope a: %.5f L/min/Hz\n', a);
fprintf('Intercept b: %.5f L/min\n', b);
fprintf('R^2: %.5f\n', r_squared);
fprintf('Max Error: %.4f L/min (%.2f%% FS)\n', max_err, (max_err/6.0)*100);
fprintf('Avg Error: %.4f L/min (%.2f%% FS)\n', avg_err, (avg_err/6.0)*100);

fig = figure('Visible', 'off', 'Position', [100, 100, 700, 450]);
scatter(f, Q, 70, 'filled', 'MarkerFaceColor', [0.85, 0.1, 0.1], 'DisplayName', 'Calibration Trials');
hold on;
f_fit = linspace(min(f)*0.8, max(f)*1.1, 100);
plot(f_fit, polyval(p, f_fit), 'b--', 'LineWidth', 2, ...
    'DisplayName', sprintf('Fit: Q = %.4ff + (%.4f) [R^2 = %.4f]', a, b, r_squared));
grid on;
set(gca, 'GridLineStyle', '--', 'GridAlpha', 0.6);
xlabel('Pulse Frequency f (Hz)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Volumetric Flow Rate Q (L/min)', 'FontSize', 11, 'FontWeight', 'bold');
title('FT-01 Flow Sensor Calibration (Q vs Frequency)', 'FontSize', 13, 'FontWeight', 'bold');
legend('Location', 'northwest', 'FontSize', 10);

out_dir = fullfile(script_dir, '..', 'media');
if ~exist(out_dir, 'dir')
    mkdir(out_dir);
end
out_img = fullfile(out_dir, 'sensor_calibration_curve.png');
saveas(fig, out_img);
fprintf('Plot saved to: %s\n', out_img);
