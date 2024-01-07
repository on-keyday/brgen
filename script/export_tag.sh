
read RELEASE_TAG < ./script/tag.txt

if [ -z "$RELEASE_TAG" ]; then
    echo "No tag found"
    exit 1
fi

echo "Exporting tag $RELEASE_TAG"
export RELEASE_TAG
