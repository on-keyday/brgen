
read TAG < tag.txt

if [ -z "$TAG" ]; then
    echo "No tag found"
    exit 1
fi

echo "Exporting tag $TAG"
export TAG
