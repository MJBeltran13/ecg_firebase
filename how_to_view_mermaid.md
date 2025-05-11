# How to View Mermaid Diagrams

You have two Mermaid flowcharts:
1. `ecg_system_flowchart.md` - Detailed version
2. `ecg_system_simple_flowchart.md` - Simplified version

Here are several ways to view these diagrams:

## Option 1: Online Mermaid Live Editor

1. Go to [https://mermaid.live](https://mermaid.live)
2. Copy the Mermaid code (everything between the triple backticks that starts with `flowchart TD`)
3. Paste it into the editor panel (left side)
4. The rendered diagram will appear on the right side
5. You can download the image using the "Download" button

## Option 2: VS Code Extension

1. Install the "Markdown Preview Mermaid Support" extension:
   - Open VS Code
   - Go to Extensions (Ctrl+Shift+X)
   - Search for "Markdown Preview Mermaid Support"
   - Click Install

2. Open your markdown file in VS Code
3. Press Ctrl+Shift+V to open the Markdown preview

## Option 3: GitHub Viewing

1. Create a GitHub repository (if you don't have one)
2. Push your markdown files to the repository
3. GitHub will automatically render the Mermaid diagrams when viewing the files

## Option 4: Install Mermaid CLI

```bash
# Using npm (Node.js package manager)
npm install -g @mermaid-js/mermaid-cli

# Then generate PNG images:
mmdc -i ecg_system_flowchart.md -o ecg_flowchart.png
mmdc -i ecg_system_simple_flowchart.md -o ecg_simple_flowchart.png
```

## Option 5: Use a Mermaid-compatible Markdown Editor

Several markdown editors support Mermaid diagrams:
- Typora
- Obsidian
- Notion

## Option 6: Online Markdown Editors

- [StackEdit](https://stackedit.io/)
- [HackMD](https://hackmd.io/)
- [Dillinger](https://dillinger.io/)

Simply paste your entire markdown file (including the Mermaid code) and they will render the diagram.

## Option 7: Browser Extensions

- Chrome: "Mermaid Diagrams" extension
- Firefox: "Mermaid Viewer" extension

These allow you to paste Mermaid code and view the rendered diagrams directly in your browser. 