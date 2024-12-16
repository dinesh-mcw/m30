# validate_refs_to_json.py
import requests
import sys
import json
import os


# Bitbucket API credentials and repository slugs
BITBUCKET_USERNAME = os.getenv("BITBUCKET_USERNAME_SYNC_SOURCE_CI")
BITBUCKET_APP_PASSWORD = os.getenv("BITBUCKET_APP_PASSWORD_SYNC_SOURCE_CI")
REPO_OWNER = "lumotive"
base_api_url = "https://api.bitbucket.org/2.0/repositories"

repo_slugs = {
    "meta_lumotive": "meta-lumotive",
    "cobra_raw2depth": "cobra_raw2depth",
    "cobra_system_control": "cobra_system_control",
    "cobra_lidar_api": "cobra_lidar_api",
    "cobra_gui" : "cobra_lidar_api"
}

def validate_git_ref(repo_slug, ref):
    session = requests.Session()
    session.auth = (BITBUCKET_USERNAME, BITBUCKET_APP_PASSWORD)
    
    # Check branch
    response = session.get(f"{base_api_url}/{REPO_OWNER}/{repo_slug}/refs/branches/{ref}")
    if response.status_code == 200:
        return "branch"
    
    # Check tag
    response = session.get(f"{base_api_url}/{REPO_OWNER}/{repo_slug}/refs/tags/{ref}")
    if response.status_code == 200:
        return "tag"

    # Split commit comma separated from the branch name
    ref = ref.split(",")[0]
    response = session.get(f"{base_api_url}/{REPO_OWNER}/{repo_slug}/commit/{ref}")
    if response.status_code == 200:
        return "commit"

    return None

def main(refs,validated_refs_json_path):
    validated_refs = {}
    for repo, ref in zip(repo_slugs.keys(), refs):
        repo_slug = repo_slugs[repo]
        ref_type = validate_git_ref(repo_slug, ref)
        if not ref_type:
            print(f"Invalid reference {ref} for repository {repo}")
            sys.exit(1)
        validated_refs[repo] = {"type": ref_type, "value": ref}
    
    with open(validated_refs_json_path + "/validated_refs.json", 'w') as f:
        json.dump( validated_refs, f, indent=4)

if __name__ == "__main__":
    refs = sys.argv[1:6]  # Command-line arguments for Git refs
    validated_refs_json_path = str(sys.argv[6:][0])  # Command-line argument for output file path
    main(refs,validated_refs_json_path)
