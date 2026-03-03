#!/usr/bin/env python3
"""
Blueprint T3D Parser
Lit les exports T3D d'UE5 et génère un pseudocode lisible par fonction.

Usage:
    python parse_blueprint.py <file.T3D>                     → liste les fonctions
    python parse_blueprint.py <file.T3D> <FunctionName>      → pseudocode de la fonction
    python parse_blueprint.py <file.T3D> --all               → pseudocode de tout

Export T3D depuis UE Editor: clic droit sur l'asset → Asset Actions → Export...
"""

import re
import sys
import io
from collections import defaultdict

# Force UTF-8 output on Windows
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')


# ===============================================================================
# LECTURE DU FICHIER
# ===============================================================================

def read_t3d(path):
    """Lit un T3D en détectant automatiquement UTF-16 ou UTF-8."""
    with open(path, 'rb') as f:
        bom = f.read(4)
    enc = 'utf-16' if bom[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8'
    with open(path, encoding=enc, errors='replace') as f:
        return f.read()


# ===============================================================================
# PARSING T3D → DICT DE NŒUDS PAR FONCTION
# ===============================================================================

def parse_t3d(content):
    """
    Retourne: { func_name: { node_name: node_dict } }

    node_dict = {
        'name': str,
        'type': str,             # 'call_func', 'var_set', 'branch', ...
        'function_name': str,    # pour K2Node_CallFunction
        'variable_name': str,    # pour K2Node_VariableGet/Set
        'variable_scope': str,   # 'local' si scope présent
        'macro_name': str,       # pour K2Node_MacroInstance
        'operation': str,        # pour K2Node_PromotableOperator
        'cast_target': str,      # pour K2Node_DynamicCast
        'member_parent': str,    # classe parente de la fonction appelée
        'enabled_state': str,    # 'DevelopmentOnly' pour les noeuds DEBUG
        'pins': { pin_id: pin_dict },
        'pins_by_name': { pin_name: pin_dict },
    }

    pin_dict = {
        'id': str, 'name': str, 'is_output': bool,
        'linked_to': [{'node': str, 'pin_id': str}],
        'default': str, 'hidden': bool,
    }
    """
    functions = {}

    # Trouver tous les blocs Begin Object avec ExportPath (contenant de vrais pins)
    # Format ExportPath: "ClassName'/Package.Blueprint:FunctionName.NodeName'"
    # On ne garde que les blocs qui ont du contenu (MacroGraphReference, CustomProperties, etc.)
    export_re = re.compile(
        r'Begin Object (?:Class=\S+ )?Name="([^"]+)" ExportPath="[^"]*:(\w+)\.([^"\']+)[\'"]',
    )

    # Découper le contenu en blocs délimités par "Begin Object Name=..." jusqu'au prochain
    # On utilise une approche ligne par ligne
    lines = content.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        m = export_re.search(line)
        if m and 'Begin Object' in line:
            node_name = m.group(1)
            func_name = m.group(2)
            # Extraire le contenu du bloc jusqu'à "End Object"
            block_lines = [line]
            i += 1
            while i < len(lines) and 'End Object' not in lines[i]:
                block_lines.append(lines[i])
                i += 1
            block_lines.append(lines[i] if i < len(lines) else 'End Object')
            block = '\n'.join(block_lines)

            # Ignorer les blocs vides (juste la déclaration de classe sans pins)
            if 'CustomProperties' not in block and 'MacroGraphReference' not in block \
               and 'FunctionReference' not in block and 'VariableReference' not in block:
                i += 1
                continue

            node = _parse_node_block(node_name, func_name, block)
            if func_name not in functions:
                functions[func_name] = {}
            functions[func_name][node_name] = node
        i += 1

    return functions


def _parse_node_block(node_name, func_name, block):
    """Parse un bloc de nœud en node_dict."""
    node = {
        'name': node_name,
        'func': func_name,
        'type': _node_type(node_name),
        'pins': {},
        'pins_by_name': {},
    }

    def extract(pattern, default=''):
        m = re.search(pattern, block)
        return m.group(1).strip() if m else default

    node['function_name'] = extract(r'FunctionReference=\([^)]*MemberName="([^"]+)"')
    node['variable_name'] = extract(r'VariableReference=\([^)]*MemberName="([^"]+)"')
    node['variable_scope'] = extract(r'MemberScope="([^"]+)"')
    node['macro_name']     = extract(r'MacroGraph="[^"]*StandardMacros:([^"\']+)[\'"]')
    node['cast_target']    = extract(r'TargetType="[^"]*[.\'/]([^"\'./]+)_C[\'"]')
    node['operation']      = extract(r'OperationName="([^"]+)"')
    node['enabled_state']  = extract(r'EnabledState=(\S+)')
    node['member_parent']  = extract(r'MemberParent="[^"]*[.\'/]([^"\'./]+)[\'"]')
    node['node_pos_x']     = extract(r'NodePosX=(-?\d+)', '0')

    # Parser les pins (chaque CustomProperties Pin est sur une seule longue ligne)
    for pm in re.finditer(r'CustomProperties Pin \((.+?)\)(?=\s*$|\s*CustomProperties)', block, re.MULTILINE):
        pin_content = pm.group(1)
        pin = _parse_pin(pin_content)
        if pin:
            node['pins'][pin['id']] = pin
            # Pour les split-pins (Array Element_Location), on garde les deux noms
            node['pins_by_name'][pin['name']] = pin
            # Alias sans suffix (_Location, _Rotation, _Scale)
            base = re.sub(r'_(Location|Rotation|Scale3?D?|X|Y|Z)$', '', pin['name'])
            if base != pin['name']:
                node['pins_by_name'].setdefault(base, pin)

    return node


def _parse_pin(content):
    """Parse le contenu d'un CustomProperties Pin (...)."""
    def get(key, default=''):
        m = re.search(rf'{key}="([^"]*)"', content)
        return m.group(1) if m else default

    def get_flag(key):
        m = re.search(rf'{key}=(True|False)', content)
        return m.group(1) == 'True' if m else False

    pin_id = re.search(r'PinId=([A-F0-9]+)', content)
    if not pin_id:
        return None

    # LinkedTo: "NodeName PinId, NodeName PinId, ..."
    linked_raw = re.search(r'LinkedTo=\(([^)]+)\)', content)
    linked_to = []
    if linked_raw:
        for lt in re.finditer(r'(\w+)\s+([A-F0-9]+)', linked_raw.group(1)):
            linked_to.append({'node': lt.group(1), 'pin_id': lt.group(2)})

    return {
        'id':        pin_id.group(1),
        'name':      get('PinName'),
        'is_output': 'EGPD_Output' in content,
        'linked_to': linked_to,
        'default':   get('DefaultValue'),
        'hidden':    get_flag('bHidden'),
        'type_cat':  get('PinType.PinCategory'),
    }


def _node_type(name):
    checks = [
        ('FunctionEntry',       'entry'),
        ('FunctionResult',      'return'),
        ('MapForEach',          'map_foreach'),
        ('MacroInstance',       'macro'),
        ('CallArrayFunction',   'array_func'),
        ('CallFunction',        'call_func'),
        ('VariableGet',         'var_get'),
        ('VariableSet',         'var_set'),
        ('IfThenElse',          'branch'),
        ('PromotableOperator',  'operator'),
        ('DynamicCast',         'cast'),
        ('Knot',                'knot'),
        ('Message',             'message'),
        ('Self',                'self'),
        ('FormatText',          'format_text'),
        ('Comment',             'comment'),
    ]
    for fragment, t in checks:
        if fragment in name:
            return t
    return 'unknown'


# ===============================================================================
# RÉSOLUTION DES CONNEXIONS
# ===============================================================================

def get_pin(node, name):
    """Retourne le pin par nom, ou None."""
    return node['pins_by_name'].get(name)


def follow_exec(node, nodes, pin_name='then'):
    """Suit un pin exec output → retourne le nœud suivant ou None."""
    pin = get_pin(node, pin_name)
    if pin and pin['linked_to']:
        return nodes.get(pin['linked_to'][0]['node'])
    return None


def resolve_value(node, pin_name, nodes, depth=0, _seen=None):
    """
    Remonte la chaîne de données pour obtenir une étiquette lisible
    de la valeur fournie sur le pin 'pin_name' du nœud.
    """
    if _seen is None:
        _seen = set()
    node_key = (node['name'], pin_name)
    if depth > 4 or node_key in _seen:
        return node.get('function_name') or node.get('variable_name') or '...'
    _seen = _seen | {node_key}

    pin = get_pin(node, pin_name)
    if not pin:
        return '?'

    if not pin['linked_to']:
        default = pin.get('default', '')
        if default and default not in ('0', '0, 0, 0', 'false', 'true', '0.000000'):
            return default
        return pin_name  # nom du pin comme placeholder

    src_name = pin['linked_to'][0]['node']
    src_pid  = pin['linked_to'][0]['pin_id']
    src = nodes.get(src_name)
    if not src:
        return '?'

    src_pin = src['pins'].get(src_pid, {})
    src_pname = src_pin.get('name', '?')

    t = src['type']
    var = src.get('variable_name', '')
    fn  = src.get('function_name', '')

    if t == 'var_get':
        owner = _var_owner(src, nodes, depth)
        return f"{owner}.{var}" if owner else var

    if t == 'var_set':
        # Output_Get pin d'un VariableSet retourne la valeur fraîchement settée
        return var

    if t == 'entry':
        return src_pname  # paramètre de la fonction

    if t == 'knot':
        return resolve_value(src, 'InputPin', nodes, depth + 1)

    if t == 'macro':
        if 'Element' in src_pname:
            arr = resolve_value(src, 'Array', nodes, depth + 1)
            return f"elem({arr})"
        return src_pname

    if t == 'map_foreach':
        if 'Key' in src_pname:   return 'map_key'
        if 'Value' in src_pname: return 'map_value'
        return 'map_elem'

    if t == 'call_func' or t == 'array_func':
        target = _call_target(src, nodes, depth)
        short_fn = fn.replace('K2_', '')
        return f"{target}.{short_fn}()" if target else f"{short_fn}()"

    if t == 'message':
        target = _call_target(src, nodes, depth)
        return f"{target}.{fn}()" if target else f"{fn}()"

    if t == 'operator':
        return _build_condition(src, nodes, depth)

    if t == 'cast':
        obj = resolve_value(src, 'Object', nodes, depth + 1)
        return f"{obj} as {src.get('cast_target', '?')}"

    if t == 'self':
        return 'self'

    return f"{src_name}.{src_pname}"


def _var_owner(node, nodes, depth):
    """Retourne le propriétaire d'une variable Get ('' = self/local)."""
    self_pin = get_pin(node, 'self')
    if self_pin and self_pin['linked_to']:
        src_name = self_pin['linked_to'][0]['node']
        src_pid  = self_pin['linked_to'][0]['pin_id']
        src = nodes.get(src_name)
        if src:
            src_pin = src['pins'].get(src_pid, {})
            return resolve_value(src, src_pin.get('name', '?'), nodes, depth + 1)
    scope = node.get('variable_scope', '')
    return '' if scope else 'self'


def _call_target(node, nodes, depth):
    """Retourne la cible d'un appel de fonction (ou '' si self)."""
    self_pin = get_pin(node, 'self')
    if self_pin and not self_pin['hidden'] and self_pin['linked_to']:
        src_name = self_pin['linked_to'][0]['node']
        src_pid  = self_pin['linked_to'][0]['pin_id']
        src = nodes.get(src_name)
        if src:
            src_pin = src['pins'].get(src_pid, {})
            val = resolve_value(src, src_pin.get('name', '?'), nodes, depth + 1)
            if val not in ('self', '?', ''):
                return val
    return ''


def _build_condition(op_node, nodes, depth):
    """Construit une chaîne de condition pour un PromotableOperator."""
    op_symbols = {
        'Less': '<', 'LessEqual': '<=', 'Greater': '>', 'GreaterEqual': '>=',
        'Equal': '==', 'NotEqual': '!=', 'Add': '+', 'Subtract': '-',
        'Multiply': '*', 'Divide': '/', 'And': '&&', 'Or': '||',
    }
    sym = op_symbols.get(op_node.get('operation', '?'), op_node.get('operation', '?'))
    a = resolve_value(op_node, 'A', nodes, depth + 1)
    b_pin = get_pin(op_node, 'B')
    b = b_pin.get('default', '?') if b_pin and not b_pin['linked_to'] else \
        resolve_value(op_node, 'B', nodes, depth + 1)
    return f"{a} {sym} {b}"


# ===============================================================================
# GÉNÉRATEUR DE PSEUDOCODE
# ===============================================================================

def generate_pseudocode(nodes, indent=0):
    """Génère le pseudocode en suivant la chaîne d'exécution depuis l'entry."""
    entry = next((n for n in nodes.values() if n['type'] == 'entry'), None)
    if not entry:
        return ['  [pas de nœud entry trouvé]']

    first = follow_exec(entry, nodes)
    if not first:
        return ['  [fonction vide]']

    # Paramètres d'entrée
    lines = []
    params = [p['name'] for p in entry['pins'].values()
              if p['is_output'] and p['name'] != 'then' and not p['hidden']]
    if params:
        lines.append(f"{'  '*indent}PARAMS: {', '.join(params)}")

    lines += _trace(first, nodes, indent + 1, set())
    return lines


def _trace(node, nodes, indent, visited):
    """Trace récursive la chaîne d'exécution."""
    lines = []
    pad = '  ' * indent
    current = node

    while current:
        cname = current['name']
        if cname in visited:
            lines.append(f"{pad}↩ [retour à {cname}]")
            break
        visited = visited | {cname}

        t = current['type']

        # ── Nœuds à ignorer / pass-through ──────────────────────────────────
        if t == 'knot':
            out = get_pin(current, 'OutputPin')
            if out and out['linked_to']:
                current = nodes.get(out['linked_to'][0]['node'])
            else:
                break
            continue

        if t in ('comment', 'unknown'):
            current = follow_exec(current, nodes)
            continue

        if t == 'return':
            lines.append(f"{pad}RETURN")
            break

        if t == 'entry':
            current = follow_exec(current, nodes)
            continue

        # ── SET variable ─────────────────────────────────────────────────────
        if t == 'var_set':
            var = current.get('variable_name', '?')
            val = resolve_value(current, var, nodes)
            is_debug = current.get('enabled_state') == 'DevelopmentOnly'
            suffix = '  [DEBUG]' if is_debug else ''
            lines.append(f"{pad}SET {var} = {val}{suffix}")
            current = follow_exec(current, nodes)
            continue

        # ── Appel de fonction ────────────────────────────────────────────────
        if t in ('call_func', 'array_func', 'message'):
            lines += _render_call(current, nodes, pad)
            current = follow_exec(current, nodes)
            continue

        # ── Cast dynamique ───────────────────────────────────────────────────
        if t == 'cast':
            obj = resolve_value(current, 'Object', nodes)
            target = current.get('cast_target', '?')
            lines.append(f"{pad}CAST {obj} → {target}")
            then_node = follow_exec(current, nodes, 'then')
            if then_node:
                lines += _trace(then_node, nodes, indent, set(visited))
            break

        # ── IF / Branch ──────────────────────────────────────────────────────
        if t == 'branch':
            cond_pin = get_pin(current, 'Condition')
            cond_str = '?'
            if cond_pin and cond_pin['linked_to']:
                src = nodes.get(cond_pin['linked_to'][0]['node'])
                if src:
                    cond_str = _build_condition(src, nodes, 0) if src['type'] == 'operator' \
                               else resolve_value(current, 'Condition', nodes)

            lines.append(f"{pad}IF {cond_str}:")
            true_node  = follow_exec(current, nodes, 'then')
            false_node = follow_exec(current, nodes, 'else')
            if true_node:
                lines.append(f"{pad}  TRUE →")
                lines += _trace(true_node, nodes, indent + 2, set(visited))
            if false_node:
                lines.append(f"{pad}  FALSE →")
                lines += _trace(false_node, nodes, indent + 2, set(visited))
            break

        # ── ForEach (MacroInstance) ──────────────────────────────────────────
        if t == 'macro':
            macro = current.get('macro_name', '?')
            arr_src = resolve_value(current, 'Array', nodes)

            # Nom de l'élément (souvent renvoyé vers un VariableSet)
            elem_info = _elem_label(current, nodes)

            if 'ForEach' in macro:
                lines.append(f"{pad}FOR EACH {arr_src}:  (elem → {elem_info})")
            else:
                lines.append(f"{pad}MACRO {macro}({arr_src}):")

            body = follow_exec(current, nodes, 'LoopBody')
            if body:
                lines += _trace(body, nodes, indent + 1, set(visited))

            completed = follow_exec(current, nodes, 'Completed')
            if completed:
                lines += _trace(completed, nodes, indent, set(visited))
            break

        # ── MapForEach ───────────────────────────────────────────────────────
        if t == 'map_foreach':
            # MapPin = nom réel du pin de la map (K2Node_MapForEach)
            map_src = resolve_value(current, 'MapPin', nodes)
            if map_src == 'MapPin':
                map_src = resolve_value(current, 'Map', nodes)
            key_info = _output_label(current, nodes, 'Key')
            val_info = _output_label(current, nodes, 'Value')
            lines.append(f"{pad}FOR EACH MAP {map_src}:")
            if key_info: lines.append(f"{pad}  KEY   → {key_info}")
            if val_info: lines.append(f"{pad}  VALUE → {val_info}")

            # LoopBody peut être nommé 'LoopBody' ou 'then' selon la version UE
            body = follow_exec(current, nodes, 'LoopBody') or \
                   follow_exec(current, nodes, 'then')
            if body:
                lines += _trace(body, nodes, indent + 1, set(visited))

            completed = follow_exec(current, nodes, 'Completed')
            if completed:
                lines += _trace(completed, nodes, indent, set(visited))
            break

        # ── Fallback ─────────────────────────────────────────────────────────
        label = _node_label(current)
        lines.append(f"{pad}{label}")
        current = follow_exec(current, nodes)

    return lines


def _render_call(node, nodes, pad):
    """Génère la ligne pour un appel de fonction."""
    fn = node.get('function_name', '?').replace('K2_', '')
    is_debug = node.get('enabled_state') == 'DevelopmentOnly'
    suffix = '  [DEBUG]' if is_debug else ''

    # Construire la cible
    target = _call_target(node, nodes, 0)

    # Arguments non-exec, non-self, non-hidden
    args = {}
    for pin in node['pins'].values():
        if pin['hidden'] or pin['is_output']: continue
        n = pin['name']
        if n in ('execute', 'self', 'WorldContextObject', 'Target'): continue
        val = resolve_value(node, n, nodes)
        if val and val != n:
            args[n] = val

    arg_str = ', '.join(f"{k}={v}" for k, v in args.items())
    if target:
        call = f"{target}.{fn}({arg_str})"
    else:
        call = f"{fn}({arg_str})"

    # Sorties connectées
    out_pins = [p for p in node['pins'].values()
                if p['is_output'] and p['name'] != 'then'
                and not p['hidden'] and p['linked_to']]
    if len(out_pins) == 1:
        return [f"{pad}{out_pins[0]['name']} = {call}{suffix}"]

    return [f"{pad}{call}{suffix}"]


def _elem_label(macro_node, nodes):
    """Retourne le nom de la variable où va l'élément du ForEach."""
    for pname, pin in macro_node['pins_by_name'].items():
        if 'Element' in pname and pin['is_output'] and not pin['hidden']:
            if '_Location' in pname or '_Rotation' in pname or '_Scale' in pname:
                continue
            for lt in pin['linked_to']:
                dest = nodes.get(lt['node'])
                if dest and dest['type'] == 'var_set':
                    return dest.get('variable_name', pname)
                if dest:
                    return dest['name']
            return pname
    return '?'


def _output_label(node, nodes, key):
    """Retourne la destination d'un pin output contenant 'key'."""
    for pname, pin in node['pins_by_name'].items():
        if key in pname and pin['is_output'] and pin['linked_to']:
            dest = nodes.get(pin['linked_to'][0]['node'])
            if dest and dest['type'] == 'var_set':
                return dest.get('variable_name', pname)
    return ''


def _node_label(node):
    t = node['type']
    if t == 'call_func':  return f"CALL {node.get('function_name','?')}()"
    if t == 'var_get':    return f"GET {node.get('variable_name','?')}"
    if t == 'var_set':    return f"SET {node.get('variable_name','?')}"
    if t == 'operator':   return f"OP {node.get('operation','?')}"
    return f"[{t}] {node['name']}"


# ===============================================================================
# MAIN
# ===============================================================================

SKIP_FUNCS = {
    'EventGraph', 'ConstructionScript', 'UserConstructionScript',
}

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    path = sys.argv[1]
    target = sys.argv[2] if len(sys.argv) > 2 else None

    print(f"Lecture de {path}...")
    content = read_t3d(path)
    print("Parsing...")
    functions = parse_t3d(content)

    # Filtrer les graphes internes
    funcs = {k: v for k, v in functions.items() if k not in SKIP_FUNCS}

    if target is None:
        print(f"\n{len(funcs)} fonctions trouvées :")
        for fn in sorted(funcs):
            n_nodes = len(funcs[fn])
            print(f"  {fn}  ({n_nodes} nœuds)")
        print(f"\nUsage: python parse_blueprint.py \"{path}\" <FunctionName>")
        print(f"       python parse_blueprint.py \"{path}\" --all")
        return

    if target == '--all':
        for fn in sorted(funcs):
            _print_func(fn, funcs[fn])
        return

    if target not in funcs:
        print(f"Fonction '{target}' introuvable.")
        print("Fonctions disponibles:")
        for fn in sorted(funcs):
            print(f"  {fn}")
        return

    _print_func(target, funcs[target])


def _print_func(func_name, nodes):
    w = 64
    print(f"\n{'='*w}")
    print(f"  FUNCTION: {func_name}")
    print(f"  Nœuds: {len(nodes)}")
    print(f"{'='*w}")
    for line in generate_pseudocode(nodes):
        print(line)
    print()


if __name__ == '__main__':
    main()
