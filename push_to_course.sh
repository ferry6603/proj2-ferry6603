#!/bin/bash
# Push Battleship Project 2 to the official COMP30023 course repository.
# Requires: feit-comp30023-2026/proj2-ferry6603 exists (accept GitHub Classroom first)
#           SSH key authorized for feit-comp30023-2026 org (SSO if prompted)

set -e
USERNAME="${1:-ferry6603}"
COURSE_REPO="git@github.com:feit-comp30023-2026/proj2-${USERNAME}.git"

cd "$(dirname "$0")"

echo "Course repo: feit-comp30023-2026/proj2-${USERNAME}"
echo "Checking SSH access..."

if ! GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=accept-new -o BatchMode=yes" \
    git ls-remote "$COURSE_REPO" HEAD >/dev/null 2>&1; then
    echo ""
    echo "ERROR: Cannot access $COURSE_REPO"
    echo ""
    echo "Do these steps first:"
    echo "  1. Accept the GitHub Classroom invite for Project 2 (link on LMS/Ed)"
    echo "     This creates: feit-comp30023-2026/proj2-${USERNAME}"
    echo "  2. Authorize SSH for the org (if prompted):"
    echo "     https://github.com/settings/keys"
    echo "     -> find your key -> Configure SSO -> Authorize feit-comp30023-2026"
    echo "  3. Run this script again"
    exit 1
fi

git remote remove course 2>/dev/null || true
git remote add course "$COURSE_REPO"

echo "Pushing main branch to course repository..."
GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=accept-new" git push course main

COMMIT=$(git rev-parse HEAD)
echo ""
echo "SUCCESS — pushed to feit-comp30023-2026/proj2-${USERNAME}"
echo ""
echo "Submit this 40-character commit hash on LMS:"
echo "  $COMMIT"
echo ""
echo "Verify with:"
echo "  git clone git@github.com:feit-comp30023-2026/proj2-${USERNAME}.git proj2"
echo "  cd proj2 && git checkout $COMMIT"
