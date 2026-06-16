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

export class HUD {
    private container: HTMLElement;

    constructor(container: HTMLElement) {
        this.container = container;
    }

    render(
        hp: number,
        maxHp: number,
        gold: number,
        phase: string,
        steps: number,
        inventory: Array<{ id: string; name: string; count: number }>,
        levelInfo?: { level: number; playerKills: number; rivalKills: number; killsRequired: number },
        playerInfo?: { respawns: number },
        rivalInfo?: { hp: number; maxHp: number; respawns: number }
    ): void {
        this.container.innerHTML = '';

        const statsDiv = document.createElement('div');
        statsDiv.style.marginBottom = '12px';
        statsDiv.innerHTML = `
      <div style="color: #ff6b6b; font-weight: bold;">Human (H)</div>
      <div style="color: #888; margin-top: 4px;">
        HP: <span style="color: #4da6ff;">${hp}/${maxHp}</span>
      </div>
      <div style="color: #888;">
        Gold: <span style="color: #ffcc00;">${gold}</span>
      </div>
      <div style="color: #888;">
        Steps: <span style="color: #4da6ff;">${steps}</span>
      </div>
      <div style="color: #888;">
        Phase: <span style="color: ${phase === 'Realtime' || phase === 'player_turn' ? '#00dd00' : '#ff6b6b'}">${phase}</span>
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
      ${rivalInfo ? `
      <div style="color: #888; margin-top: 6px; border-top: 1px solid #505060; padding-top: 6px;">
        <div style="color: #00dd88; font-weight: bold;">Agent (A)</div>
        HP: <span style="color: #00dd88;">${rivalInfo.hp}/${rivalInfo.maxHp}</span><br>
        Respawns: <span style="color: #00dd88;">${rivalInfo.respawns}</span>
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
}