# Run on unictest.yaml workflow to copy test results to html file for better visualization.


def copy_test_results_to_html(test_results_path):
    with open("script/unictest_result.html", "r", encoding="utf-8") as f:
        html_content = f.read()

    MAGIC_STRING = "{%TEST_RESULTS%}"
    with open(test_results_path, "r", encoding="utf-8") as f:
        test_results_content = f.read()

    updated_html = html_content.replace(MAGIC_STRING, test_results_content)

    with open("test_results/unictest_result.html", "w", encoding="utf-8") as f:
        f.write(updated_html)


if __name__ == "__main__":
    copy_test_results_to_html("test_results/test_results.json")
