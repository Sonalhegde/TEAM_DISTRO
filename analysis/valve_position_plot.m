%% Q2.1: Ball valve position vs flow rate (FT-01, calibrated: Q = 0.009244 f + 0.040111)
% Data source: analysis/data/valve_characterization.csv
clear; clc; close all;

script_dir = fileparts(mfilename('fullpath'));
csv_file = fullfile(script_dir, 'data', 'valve_characterization.csv');

if exist(csv_file, 'file')
    T = readtable(csv_file);
    valve = T.valve_pos_pct';
    freq  = T.pulse_freq_hz';
    Q     = T.flow_rate_Lpm';
else
    valve = [25 50 75 100];
    freq  = [105.9 186.8 215.8 218.8];
    Q     = [1.019 1.767 2.035 2.063];
end

% Check the readings against the calibration equation
Q_check = 0.009244 * freq + 0.040111;
fprintf('Valve %%  Freq (Hz)  Q shown  Q calc\n');
fprintf('%5d  %9.1f  %7.3f  %6.3f\n', [valve; freq; Q; Q_check]);

media_dir = fullfile(script_dir, '..', 'media');
if ~exist(media_dir, 'dir'), mkdir(media_dir); end

fig = figure('Color','w','Position',[100 100 720 480], 'Visible', 'off');
plot(valve, Q, 'o-', 'LineWidth', 2, 'MarkerSize', 8, 'MarkerFaceColor', [0.1 0.3 0.8]);
grid on; box on; hold on;
for i = 1:numel(valve)
    text(valve(i), Q(i) + 0.08, sprintf('%.3f L/min', Q(i)), ...
         'HorizontalAlignment', 'center', 'FontWeight', 'bold');
end
xlabel('Ball valve position (%)');
ylabel('Measured flow rate (L/min)');
title('Valve position vs flow rate (Pump-2 at 100 % duty)');
xlim([15 110]); ylim([0 2.6]);
exportgraphics(fig, fullfile(media_dir, 'valve_position_vs_flow.png'), 'Resolution', 200);
