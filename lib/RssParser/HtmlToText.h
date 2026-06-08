#pragma once
#include <string>

// Convert an HTML fragment (feed description / content:encoded) to readable
// plain text: drop <script>/<style>, turn <br> into "\n" and block ends
// (</p>,</div>,</li>,</h1-6>) into "\n\n", strip remaining tags, decode the
// common named entities and numeric (&#nnn; / &#xhh;) entities, and collapse
// runs of whitespace. UTF-8 output. No heap blow-up: single pass over input.
std::string htmlToText(const std::string& html);
