import json
import re
import sys

# Recipe paths relative to the meta-lumotive directory
recipe_relative_paths = {
    "cobra_raw2depth": "recipes-system/cobra-raw2depth/cobra-raw2depth.bb",
    "cobra_system_control": "recipes-system/cobra-python/python3-cobra-system-control.bb",
    "cobra_lidar_api": "recipes-system/cobra-python/python3-cobra-lidar-api.bb",
    "cobra_gui": "recipes-system/cobra-gui/cobra-gui.bb"
}

def read_validated_refs(build_dir):
    with open(f"{build_dir}/validated_refs.json", 'r') as f:
        return json.load(f)

def update_recipe(build_dir, repo_name, ref_type, ref_value):
    if repo_name in ["meta-lumotive", "meta_lumotive"]:
        print(f"Skipping update for {repo_name} as it is the meta layer.")
        return

    recipe_path = f"{build_dir}/sources/meta-lumotive/{recipe_relative_paths[repo_name]}"
    
    try:
        with open(recipe_path, 'r') as file:
            lines = file.readlines()

        updated_lines = []
        i = 0
        is_srcrev_updated = False
        while i < len(lines):
            line = lines[i].rstrip('\n')
            if line.startswith('SRC_URI'):
                src_uri_lines = [line]
                i += 1
                while lines[i].strip().endswith('\\'):
                    src_uri_lines.append(lines[i].strip())
                    i += 1
                src_uri_lines.append(lines[i].strip())

                new_src_uri = f'SRC_URI = "git://git@bitbucket.org/lumotive/{repo_name};protocol=ssh;lfs=1;{ref_type}={ref_value}"' if repo_name != "cobra_gui" else f'SRC_URI = "git://git@bitbucket.org/lumotive/cobra_lidar_api;protocol=ssh;lfs=1;{ref_type}={ref_value}"'

                if ref_type == "commit":
                    # Remove the branch part from ref which is comma separated
                    ref_branch = ref_value.split(",")[1]
                    
                    # Reassign ref_value to the commit sha
                    ref_value = ref_value.split(",")[0]
                    
                    new_src_uri = f'SRC_URI = "git://git@bitbucket.org/lumotive/{repo_name};protocol=ssh;lfs=1;branch={ref_branch};rev=${{SRCREV}}"' if repo_name != "cobra_gui" else f'SRC_URI = "git://git@bitbucket.org/lumotive/cobra_lidar_api;protocol=ssh;lfs=1;branch={ref_branch};rev=${{SRCREV}}"'

                updated_lines.append(new_src_uri + '\n')
                i += 1  # Skip adding the current line as it's the last part of SRC_URI, already processed
                continue
            elif line.startswith('SRCREV'):
                if ref_type == "commit":
                    ref_value = ref_value.split(",")[0]
                new_src_rev = f'SRCREV = "{ref_value}"' if ref_type == "commit" else 'SRCREV = "${AUTOREV}"'
                updated_lines.append(new_src_rev + '\n')
                is_srcrev_updated = True
                i += 1
            else:
                updated_lines.append(line + '\n')
                i += 1
        
        if not is_srcrev_updated and ref_type == "commit":
            ref_value = ref_value.split(",")[0]
            updated_lines.append(f'\nSRCREV = "{ref_value}"\n')

        if not is_srcrev_updated and ref_type == "tag":
            updated_lines.append(f'\nSRCREV = "{ref_value}"\n')

        if not is_srcrev_updated and ref_type == "branch":
            updated_lines.append(f'\nSRCREV = "${{AUTOREV}}"\n')


        with open(recipe_path, 'w') as file:
            file.writelines(updated_lines)

        print(f"Updated {recipe_path} for {repo_name} with {ref_type}: {ref_value}")

    except FileNotFoundError:
        print(f"Recipe file not found: {recipe_path}")

def main(build_dir):
    validated_refs = read_validated_refs(build_dir)
    for repo_name, details in validated_refs.items():
        update_recipe(build_dir, repo_name, details["type"], details["value"])

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python update_yocto_recipes.py <build_directory>")
        sys.exit(1)
    build_dir = sys.argv[1]
    main(build_dir)
