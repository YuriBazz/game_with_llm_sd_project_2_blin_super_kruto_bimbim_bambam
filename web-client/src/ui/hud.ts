export class GameLog {
    private messages: string[] = [];
    private maxMessages: number = 10;

    add(message: string): void {
        const timestamp = new Date().toLocaleTimeString();
        this.messages.push(`[${timestamp}] ${message}`);
        if (this.messages.length > this.maxMessages) {
            this.messages.shift();
        }
    }

    getMessages(): string[] {
        return [...this.messages];
    }

    clear(): void {
        this.messages = [];
    }

    render(container: HTMLElement): void {
        container.innerHTML = '';
        const list = document.createElement('div');
        list.style.fontSize = '12px';
        list.style.fontFamily = 'monospace';
        list.style.lineHeight = '1.4';

        for (const msg of this.messages) {
            const line = document.createElement('div');
            line.textContent = msg;
            line.style.color = '#00dd00';
            line.style.marginBottom = '2px';
            list.appendChild(line);
        }

        container.appendChild(list);
    }
}

export interface AgentPanelData {
    marker: string;
    label: string;
    color: string;
    x: number;
    y: number;
    hp: number;
    maxHp: number;
    kills: number;
    killsRequired: number;
    respawns: number;
    gold: number;
    inventory: Array<{ id: string; name: string; count: number }>;
}

export interface MatchMeta {
    steps: number;
    level: number;
}

function formatInventory(
    inventory: Array<{ id: string; name: string; count: number }>
): string {
    if (inventory.length === 0) {
        return '<div style="color: #666; font-size: 11px;">(empty)</div>';
    }
    return inventory
        .map(
            (item) =>
                `<div style="color: #bbb; font-size: 11px; padding: 2px 0;">
                    <span style="color: #ffcc00;">${item.name || item.id}</span>
                    <span style="color: #888;"> ×${item.count}</span>
                    <span style="color: #555;"> (${item.id})</span>
                </div>`
        )
        .join('');
}

function renderAgentPanel(agent: AgentPanelData): string {
    const hpPct = Math.max(0, Math.min(100, Math.round((agent.hp / Math.max(1, agent.maxHp)) * 100)));
    return `
        <div style="margin-top: 10px; padding-top: 8px; border-top: 1px solid #505060;">
            <div style="display: flex; align-items: center; gap: 8px;">
                <span style="
                    display: inline-block; width: 22px; height: 22px; line-height: 22px;
                    text-align: center; font-weight: bold; border-radius: 3px;
                    background: ${agent.color}; color: #1a1a2e; font-size: 12px;
                ">${agent.marker}</span>
                <span style="color: ${agent.color}; font-weight: bold;">${agent.label}</span>
            </div>
            <div style="color: #888; margin-top: 6px; font-size: 12px;">
                Pos: <span style="color: ${agent.color};">(${agent.x}, ${agent.y})</span>
            </div>
            <div style="color: #888; margin-top: 4px; font-size: 12px;">
                HP: <span style="color: ${agent.color};">${agent.hp}/${agent.maxHp}</span>
                (${hpPct}%)
                &nbsp;|&nbsp; Kills: ${agent.kills}/${agent.killsRequired}
                &nbsp;|&nbsp; Respawns: ${agent.respawns}
            </div>
            <div style="margin-top: 6px;">
                <div style="color: #888; font-size: 11px; font-weight: bold; margin-bottom: 2px;">Inventory</div>
                ${formatInventory(agent.inventory)}
            </div>
        </div>
    `;
}

export class HUD {
    private container: HTMLElement;

    constructor(container: HTMLElement) {
        this.container = container;
    }

    render(
        hp: number,
        maxHp: number,
        _gold: number,
        _phase: string,
        steps: number,
        inventory: Array<{ id: string; name: string; count: number }>,
        levelInfo?: { level: number; playerKills: number; rivalKills: number; killsRequired: number },
        playerInfo?: { respawns: number; x?: number; y?: number },
        rivalInfo?: {
            hp: number;
            maxHp: number;
            respawns: number;
            x?: number;
            y?: number;
            gold?: number;
            inventory?: Array<{ id: string; name: string; count: number }>;
        }
    ): void {
        if (rivalInfo && levelInfo) {
            this.renderDualAgents(
                null,
                {
                    marker: 'H',
                    label: 'Robot H',
                    color: '#4da6ff',
                    x: playerInfo?.x ?? 0,
                    y: playerInfo?.y ?? 0,
                    hp,
                    maxHp,
                    kills: levelInfo.playerKills,
                    killsRequired: levelInfo.killsRequired,
                    respawns: playerInfo?.respawns ?? 0,
                    gold: _gold,
                    inventory,
                },
                {
                    marker: 'A',
                    label: 'Robot A',
                    color: '#00dd88',
                    x: rivalInfo.x ?? 0,
                    y: rivalInfo.y ?? 0,
                    hp: rivalInfo.hp,
                    maxHp: rivalInfo.maxHp,
                    kills: levelInfo.rivalKills,
                    killsRequired: levelInfo.killsRequired,
                    respawns: rivalInfo.respawns,
                    gold: rivalInfo.gold ?? 0,
                    inventory: rivalInfo.inventory ?? [],
                },
                {
                    steps,
                    level: levelInfo.level,
                }
            );
            return;
        }

        this.container.innerHTML = '';
        const statsDiv = document.createElement('div');
        statsDiv.style.marginBottom = '12px';
        statsDiv.innerHTML = `
            <div style="color: #ff6b6b; font-weight: bold;">Human (H)</div>
            <div style="color: #888; margin-top: 4px;">
                HP: <span style="color: #4da6ff;">${hp}/${maxHp}</span>
            </div>
            <div style="color: #888;">
                Steps: <span style="color: #4da6ff;">${steps}</span>
            </div>
            ${levelInfo ? `
            <div style="color: #888; margin-top: 6px; border-top: 1px solid #505060; padding-top: 6px;">
                Level: <span style="color: #e94560;">${levelInfo.level}</span><br>
                H kills: <span style="color: #4da6ff;">${levelInfo.playerKills}/${levelInfo.killsRequired}</span><br>
                A kills: <span style="color: #00dd88;">${levelInfo.rivalKills}/${levelInfo.killsRequired}</span><br>
            </div>` : ''}
            ${playerInfo ? `
            <div style="color: #888; margin-top: 6px; border-top: 1px solid #505060; padding-top: 6px;">
                H respawns: <span style="color: #4da6ff;">${playerInfo.respawns}</span>
            </div>` : ''}
        `;
        this.container.appendChild(statsDiv);

        const invDiv = document.createElement('div');
        invDiv.style.borderTop = '1px solid #505060';
        invDiv.style.paddingTop = '8px';
        invDiv.style.marginBottom = '12px';

        const invTitle = document.createElement('div');
        invTitle.style.color = '#ff6b6b';
        invTitle.style.fontWeight = 'bold';
        invTitle.textContent = 'Inventory';
        invDiv.appendChild(invTitle);

        if (inventory.length === 0) {
            const empty = document.createElement('div');
            empty.style.color = '#888';
            empty.style.marginTop = '4px';
            empty.textContent = '(empty)';
            invDiv.appendChild(empty);
        } else {
            for (const item of inventory) {
                const itemDiv = document.createElement('div');
                itemDiv.style.color = '#888';
                itemDiv.style.marginTop = '4px';
                itemDiv.style.cursor = 'pointer';
                itemDiv.style.padding = '2px 4px';
                itemDiv.style.backgroundColor = '#2d2d3d';
                itemDiv.style.borderRadius = '2px';
                itemDiv.innerHTML = `${item.name} x${item.count} <span style="color: #00dd00;">[Use]</span>`;
                itemDiv.dataset.itemId = item.id;
                invDiv.appendChild(itemDiv);
            }
        }

        this.container.appendChild(invDiv);
    }

    renderSpectator(
        spectator: { x: number; y: number },
        robotH: AgentPanelData,
        robotA: AgentPanelData,
        meta: MatchMeta
    ): void {
        this.renderDualAgents(spectator, robotH, robotA, meta);
    }

    renderDualAgents(
        spectator: { x: number; y: number } | null,
        robotH: AgentPanelData,
        robotA: AgentPanelData,
        meta: MatchMeta
    ): void {
        const observerBlock = spectator
            ? `
            <div style="margin-bottom: 10px; padding-bottom: 8px; border-bottom: 1px solid #505060;">
                <div style="color: #c77dff; font-weight: bold;">Observer (you) — O</div>
                <div style="color: #888; margin-top: 4px; font-size: 12px;">
                    Pos: <span style="color: #c77dff;">(${spectator.x}, ${spectator.y})</span>
                    &nbsp;|&nbsp; noclip, full map
                </div>
            </div>
        `
            : '';

        this.container.innerHTML = `
            ${observerBlock}
            <div style="color: #888; font-size: 12px; margin-bottom: 4px;">
                Steps: ${meta.steps}
                &nbsp;|&nbsp; Level: ${meta.level}
            </div>
            ${renderAgentPanel(robotH)}
            ${renderAgentPanel(robotA)}
        `;
    }
}