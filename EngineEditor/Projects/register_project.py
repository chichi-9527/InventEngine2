import os
import sys
import yaml

def main():
    if len(sys.argv) < 3:
        print("Usage: register_project.py <project_name> <project_path>")
        sys.exit(1)

    game_name = sys.argv[1]
    game_path = sys.argv[2]

    current_dir = os.path.dirname(os.path.abspath(__file__))
    yaml_path = os.path.join(current_dir, "projects.yaml")

    projects = []
    if os.path.exists(yaml_path):
        try:
            with open(yaml_path, "r") as f:
                content = yaml.safe_load(f)
                if isinstance(content, list):
                    projects = content
        except yaml.YAMLError as e:
            print(f"Error loading projects.yaml: {e}")
            sys.exit(1)

    # 检查项目是否已存在
    is_update = False
    for project in projects:
        if project["name"] == game_name:
            project["path"] = game_path
            is_update = True
            break

    if not is_update:
        projects.append({"name": game_name, "path": game_path})

    # 写入 projects.yaml
    try:
        with open(yaml_path, "w", encoding="utf-8") as f:
            yaml.dump(projects, f, default_flow_style=False, allow_unicode=True)
        print(f"项目 {game_name} 已注册到 projects.yaml")
        print(f"Target file: {yaml_path}")
    except Exception as e:
        print(f"Error writing projects.yaml: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
