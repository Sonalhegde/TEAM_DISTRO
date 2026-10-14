%% Ball Valve (BV-01) Throttling Characteristic Plotter
% Inputs: analysis/data/valve_characterization.csv
% Outputs: media/valve_characteristic_curve.png

clear; clc; close all;

script_dir = fileparts(mfilename('fullpath'));
data_file = fullfile(script_dir, 'data', 'valve_characterization.csv');

if ~exist(data_file, 'file')
    error('Data file not found: %s', data_file);
end

data = readtable(data_file);
pos = data.valve_pos_pct;
flow = data.flow_rate_Lpm;

fig = figure('Visible', 'off', 'Position', [100, 100, 700, 450]);
plot(pos, flow, 'bo-', 'LineWidth', 2, 'MarkerSize', 8, 'MarkerFaceColor', 'b', ...
    'DisplayName', 'Measured Flow Rate');
grid on;
set(gca, 'GridLineStyle', '--', 'GridAlpha', 0.6);
xlabel('Ball Valve Position (%)', 'FontSize', 11, 'FontWeight', 'bold');
ylabel('Measured Flow Rate (L/min)', 'FontSize', 11, 'FontWeight', 'bold');
title('Valve Position (%) vs Flow Rate (L/min)', 'FontSize', 13, 'FontWeight', 'bold');
xlim([15, 105]);
ylim([0, 3.8]);

% Annotate data points
for i = 1:length(pos)
    text(pos(i), flow(i) + 0.12, sprintf('%.2f L/min', flow(i)), ...
        'HorizontalAlignment', 'center', 'FontWeight', 'bold', 'FontSize', 9);
end

out_dir = fullfile(script_dir, '..', 'media');
if ~exist(out_dir, 'dir')
    mkdir(out_dir);
end
out_img = fullfile(out_dir, 'valve_characteristic_curve.png');
saveas(fig, out_img);
fprintf('Plot saved to: %s\n', out_img);
