%% FT-01 calibration plots (Lam Research Challenge 3.0, Stage 2)
% Data source: analysis/data/calibration_trials.csv
clear; clc; close all;

script_dir = fileparts(mfilename('fullpath'));
csv_file = fullfile(script_dir, 'data', 'calibration_trials.csv');

if exist(csv_file, 'file')
    T = readtable(csv_file);
    is_fit = strcmpi(T.role, 'fit');
    is_val = strcmpi(T.role, 'validation');
    N  = T.pulses(is_fit)';
    t  = T.time_s(is_fit)';
    V  = T.volume_L(is_fit)';
    Nv = T.pulses(is_val)';
    tv = T.time_s(is_val)';
    Vv = T.volume_L(is_val)';
else
    % Fallback hardcoded values matching data/calibration_trials.csv
    N   = [6678 5525 4316 2493 4881 6013];
    t   = [30.584 30.378 30.777 30.210 30.571 30.409];
    V   = [1.040 0.870 0.700 0.390 0.790 0.940];
    Nv = 3542; tv = 30.279; Vv = 0.560;
end

f   = N ./ t;            % Hz
Q   = V ./ t * 60;       % L/min (actual)
fv  = Nv / tv;
Qv  = Vv / tv * 60;

%% Least-squares fit  Q = a*f + b
p = polyfit(f, Q, 1);  a = p(1);  b = p(2);
Qfit = a*f + b;
R2 = 1 - sum((Q - Qfit).^2) / sum((Q - mean(Q)).^2);
err = abs(Qfit - Q) ./ Q * 100;
Qv_cal = a*fv + b;
errv = abs(Qv_cal - Qv) / Qv * 100;

fprintf('a = %.6f, b = %.6f, R^2 = %.5f\n', a, b, R2);
fprintf('Max error = %.2f %%, Avg error = %.2f %%\n', max(err), mean(err));
fprintf('Validation: Q_act = %.3f, Q_cal = %.3f, error = %.2f %%\n', Qv, Qv_cal, errv);

media_dir = fullfile(script_dir, '..', 'media');
if ~exist(media_dir, 'dir'), mkdir(media_dir); end

%% Figure 1: calibration curve
fx = linspace(0, 240, 100);
fig1 = figure('Color','w','Position',[100 100 760 520], 'Visible', 'off');
plot(f, Q, 'bo', 'MarkerFaceColor','b', 'MarkerSize',7); hold on;
plot(fx, a*fx + b, 'r-', 'LineWidth', 1.8);
plot(fv, Qv, 's', 'MarkerSize',9, 'MarkerFaceColor',[0.1 0.7 0.2], 'MarkerEdgeColor','k');
grid on; box on;
xlabel('Pulse frequency f (Hz)'); ylabel('Flow rate Q (L/min)');
title('FT-01 calibration: Q vs pulse frequency');
legend('Calibration trials', sprintf('Fit: Q = %.6f f + %.6f', a, b), 'Validation trial', 'Location','northwest');
text(120, 0.35, sprintf('R^2 = %.5f\nMax error = %.2f %%\nAvg error = %.2f %%', R2, max(err), mean(err)), ...
     'BackgroundColor','w', 'EdgeColor',[0.6 0.6 0.6]);
xlim([0 240]); ylim([0 2.4]);
exportgraphics(fig1, fullfile(media_dir, 'ft01_calibration_curve.png'), 'Resolution', 200);

%% Figure 2: calibration error vs actual flow
[Qs, idx] = sort(Q);
fig2 = figure('Color','w','Position',[100 100 760 420], 'Visible', 'off');
bar(Qs, err(idx), 0.08, 'FaceColor',[0.2 0.4 0.8]); hold on;
plot(Qv, errv, 's', 'MarkerSize',9, 'MarkerFaceColor',[0.1 0.7 0.2], 'MarkerEdgeColor','k');
yline(mean(err), 'r--', sprintf('Average %.2f %%', mean(err)));
grid on; box on;
xlabel('Actual flow Q (L/min)'); ylabel('Calibration error (%)');
title('FT-01 calibration error vs flow');
legend('Calibration trials', 'Validation trial', 'Location','northeast');
exportgraphics(fig2, fullfile(media_dir, 'ft01_calibration_error.png'), 'Resolution', 200);
